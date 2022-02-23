import { SortableMultiSelect } from "/views/options/components/sortable-multi-select/main.mjs";

import { fetchJSONAsObject, fetchHTMLAsFragment } from "/core/utils/commons.mjs";

import Command from "/core/models/command.mjs";

// getter for module path
const MODULE_DIR = (() => {
  const urlPath = new URL(import.meta.url).pathname;
  return urlPath.slice(0, urlPath.lastIndexOf("/") + 1);
})();

const COMMAND_ITEMS = fetchJSONAsObject(browser.runtime.getURL("/resources/json/commands.json"));
const COMMAND_SETTING_TEMPLATES = fetchHTMLAsFragment(browser.runtime.getURL("/views/options/fragments/command-setting-templates.inc"));

/**
 * Command Multi Select
 * This is a custom element which extends the multi select field to display all commands
 * It also handles necessary permission checks and the settings of each selected command
 **/
class CommandMultiSelect extends SortableMultiSelect {
  constructor() {
    super();

    const stylesheet = document.createElement("link");
    stylesheet.rel = "stylesheet";
    stylesheet.href = MODULE_DIR  + "layout.css";
    this.shadowRoot.append(stylesheet);

    const settingsConainer = document.createElement("div");
    settingsConainer.id = "settings";
    this.shadowRoot.append(settingsConainer);

    // build/fill command selection list
    COMMAND_ITEMS.then((commandItems) => {
      const commandMultiSelectItems = commandItems.map((commandItem) => {
        const commandMultiSelectItem = document.createElement("sortable-multi-select-item");
        commandMultiSelectItem.dataset.command = commandItem.command;
        commandMultiSelectItem.textContent = browser.i18n.getMessage(`commandLabel${commandItem.command}`);
        return commandMultiSelectItem;
      });
      this.append(...commandMultiSelectItems);
    });

    this._commandSettingsRelation = new WeakMap();
  }


  /**
   * Set the placeholder attributes from the parent (SortableMultiSelect) element
   **/
  connectedCallback () {
    this.placeholder = browser.i18n.getMessage("commandMultiSelectAddPlaceholder");
    this.dropdownPlaceholder = browser.i18n.getMessage("commandMultiSelectNoResultsPlaceholder");
  }


  /**
   * Getter for the "value" attribute
   * This returns an array of command objects
   **/
  get value () {
    const commandList = [];

    for (const item of this.shadowRoot.getElementById("items").children) {
      // create command object and add it to the return value list
      const command = new Command(item.dataset.value);
      commandList.push(command);

      const settings = this._commandSettingsRelation.get(item);
      // if no settings exist skip to next command
      if (!settings) continue;

      const settingInputs = settings.querySelectorAll("[name]");

      for (let settingInput of settingInputs) {
        let value;
        // get true or false for checkboxes
        if (settingInput.type === "checkbox") {
          value = settingInput.checked;
        }
        // get value as number for number fields
        else if (!isNaN(settingInput.valueAsNumber)) {
          value = settingInput.valueAsNumber;
        }
        else {
          value = settingInput.value;
        }
        command.setSetting(settingInput.name, value);
      }
    }

    return commandList;
  }


  /**
   * Setter for the "value" attribute
   * This requires an array of command objects or Commands
   **/
  set value (value) {
    // remove all previous items and settings
    const items = this.shadowRoot.getElementById("items");
    while (items.firstChild) items.firstChild.remove();
    const settingsConainer = this.shadowRoot.getElementById("settings");
    while (settingsConainer.firstChild) settingsConainer.firstChild.remove();
    // add new command items and settings
    for (let command of value) {
      // convert command JSON object to Command
      if (!(command instanceof Command)) {
        command = new Command(command);
      }
      this._createCommandItem(command);
    }
  }


  /**
   * Constructs a command item and its settings if any
   * The item is immediately appended to the selection
   * Stores the settings html in the _commandSettingsRelation map
   * Returns the command item html element
   **/
  async _createCommandItem (command) {
    const items = this.shadowRoot.getElementById("items");
    const commandItem = this._buildSelectedItem(command);
    items.append(commandItem);

    // if the command offers any settings create the necessary inputs
    if (command.hasSettings()) {
      const settingsConainer = this.shadowRoot.getElementById("settings");
      const settingsPanel = await this._buildSettingsPanel(command);
      settingsConainer.append(settingsPanel);
      // save relation/mapping of command and its setting elements
      this._commandSettingsRelation.set(commandItem, settingsPanel);
    }
    return commandItem;
  }


  /**
   * Select given command item and shows its settings if any
   * This automatically unselectes the previous command
   **/
  _selectCommandItem (commandItem) {
    // unmark current command if any
    const currentlySelectedCommandItem = this.shadowRoot.querySelector("#items .item.selected");
    if (currentlySelectedCommandItem) {
      currentlySelectedCommandItem.classList.remove("selected");
      // hide settings if available
      const currentlySelectedCommandSettings = this._commandSettingsRelation.get(currentlySelectedCommandItem);
      if (currentlySelectedCommandSettings) {
        currentlySelectedCommandSettings.hidden = true;
      }
    }
    // mark new command as active
    if (commandItem) {
      commandItem.classList.add("selected");
      // show settings if available
      const newlySelectedCommandSettings = this._commandSettingsRelation.get(commandItem);
      if (newlySelectedCommandSettings) {
        newlySelectedCommandSettings.hidden = false;
      }
    }
  }


  /**
   * Builds the required item html for the given command and returns its
   **/
  _buildSelectedItem (command) {
    // call parent methode
    const item = super._buildSelectedItem(command.getName(), command.toString());
    item.addEventListener("focusin", this);
    return item;
  }


  /**
   * Builds the required settings panel html for the given command and returns its
   **/
  async _buildSettingsPanel (command) {
    const settingsPanel = document.createElement("div");
    settingsPanel.class = "cms-settings-panel";
    settingsPanel.hidden = true;

    const settingTemplates = await COMMAND_SETTING_TEMPLATES;

     // build and insert the corresponding setting templates
    for (let template of settingTemplates.querySelectorAll(`[data-commands~="${command.getName()}"]`)) {
      const settingContainer = document.createElement("div");
            settingContainer.classList.add("cb-setting");
      const setting = document.importNode(template.content, true);
      // append the current settings
      settingContainer.append(setting);
      settingsPanel.append(settingContainer);
    }

    // insert text from language files
    for (let element of settingsPanel.querySelectorAll('[data-i18n]')) {
      element.textContent = browser.i18n.getMessage(element.dataset.i18n);
    }

    // insert command settings
    for (let settingInput of settingsPanel.querySelectorAll("[name]")) {
      if (command.hasSetting(settingInput.name)) {
        if (settingInput.type === "checkbox") {
          settingInput.checked = command.getSetting(settingInput.name);;
        }
        else {
          settingInput.value = command.getSetting(settingInput.name);;
        }
      }
    }

    return settingsPanel;
  }


  /**
   * Handles the command selection procedure
   * Asks the user for extra permissions if required
   * Switches to the command settings if existing
   **/
  async _handleDropdownClick (event) {
    const dropdown = this.shadowRoot.getElementById("dropdown");
    // get closest slotted element
    const commandSelectItem = event.composedPath().find((ele) => ele.assignedSlot === dropdown);

    if (commandSelectItem) {
      // get command object
      const commandObject = (await COMMAND_ITEMS).find((element) => {
        return element.command === commandSelectItem.dataset.command;
      });

      if (commandObject) {
        // if the command requires permissions
        if (commandObject.permissions) {
          const permissionRequest = browser.permissions.request({
            permissions: commandObject.permissions,
          });
          // exit if permissions aren't granted
          if (!await permissionRequest) return;
        }

        const command = new Command(commandObject.command, commandObject.settings);
        const commandItem = await this._createCommandItem(command);
        commandItem.classList.add("animate");
        // mark new command item as active and show settings
        this._selectCommandItem(commandItem);
      }
    }
  }


  /**
   * Handles command item focus
   * Selects the command and hides the dropdown
   **/
  _handleItemFocusin (event) {
    // mark command item as active and show settings
    this._selectCommandItem(event.currentTarget);
    this._hideDropdown();
  }


  /**
   * Handle item remove button
   * Removes the command and settings html
   **/
  _handleItemRemoveButtonClick (event) {
    const item = event.target.closest(".item");
    // call parent methode
    super._handleItemRemoveButtonClick(event);
    // remove settings
    this._commandSettingsRelation.get(item)?.remove();
  }
}

// define custom element <command-multi-select></command-multi-select>
window.customElements.define('command-multi-select', CommandMultiSelect);







