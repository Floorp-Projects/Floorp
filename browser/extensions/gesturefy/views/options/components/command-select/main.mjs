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
 * Custom element - <command-select>
 * Accepts one special attribute (and property):
 * value : Instance of class Command
 * Dispatches a "change" event on value changes
 **/
class CommandSelect extends HTMLElement {

  /**
   * Construcor
   * Set default variables
   * Create shadow root and load stylesheet by appending it to the shadow DOM
   * If available set command select text and title
   **/
  constructor() {
    super();
    // holds the active command object
    this._command = null;
    // holds a reference of the current command object when switching to the settings panel
    this._selectedCommand = null;
    // holds the current scroll position when switching to the settings panel
    this._scrollPosition = 0;

    this.attachShadow({mode: 'open'}).innerHTML = `
      <link rel="stylesheet" href="${MODULE_DIR}layout.css">
      <button id="mainAccessButton"></button>
      <button id="secondaryAccessButton"></button>
    `;

    const mainAccessButton = this.shadowRoot.getElementById("mainAccessButton");
    mainAccessButton.addEventListener('click', this._handleMainButtonClick.bind(this));
    const secondaryAccessButton = this.shadowRoot.getElementById("secondaryAccessButton");
    secondaryAccessButton.addEventListener('click', this._handleSecondaryButtonClick.bind(this));
  }


  /**
   * Add attribute observer
   **/
  static get observedAttributes() {
    return ['value'];
  }


  /**
   * Attribute change handler
   * Change the text and title of the command select on "value" attribute change
   **/
  attributeChangedCallback (name, oldValue, newValue) {
    if (name === "value") {
      const mainAccessButton = this.shadowRoot.getElementById("mainAccessButton");
      const secondaryAccessButton = this.shadowRoot.getElementById("secondaryAccessButton");

      // don't use the command setter to prevent loop
      if (newValue) {
        this._command = new Command(this.value);
      }
      else {
        this._command = null;
      }

      if (this.command instanceof Command) {
        mainAccessButton.textContent = mainAccessButton.title = this.command.toString();
        const hasSettings = this._command.hasSettings();
        secondaryAccessButton.classList.toggle("has-settings", hasSettings);
        secondaryAccessButton.title = hasSettings
        ? browser.i18n.getMessage("commandBarOpenSettingsText")
        : browser.i18n.getMessage("commandBarOpenSelectionText");
      }
      else {
        mainAccessButton.textContent = mainAccessButton.title = "";
        secondaryAccessButton.classList.remove("has-settings");
        secondaryAccessButton.title = browser.i18n.getMessage("commandBarOpenSelectionText");
      }
    }
  }


  /**
   * Getter for the "value" attribute
   **/
  get value () {
    return JSON.parse(this.getAttribute('value'));
  }


  /**
   * Setter for the "value" attribute
   **/
  set value (value) {
    this.setAttribute('value', JSON.stringify(value));
  }


  /**
   * Getter for the plain command object
   **/
  get command () {
    return this._command;
  }


  /**
   * Setter for the plain command object
   **/
  set command (value) {
    if (value instanceof Command) {
      this._command = value;
      this.setAttribute('value', JSON.stringify(value.toJSON()));
    }
    else {
      this._command = null;
      this.removeAttribute('value');
    }
  }


  /**
   * Constructs the main command bar structure and overlay
   * Returns it as a document fragment
   **/
  _buildCommandBar () {
    const templateFragment = document.createRange().createContextualFragment(`
      <div id="overlay"></div>

      <div id="commandBar">
        <button id="commandBarCancelButton" type="button"></button>
        <div id="commandBarWrapper"></div>
      </div>
    `);

    // register event handlers
    const overlay = templateFragment.getElementById("overlay");
          overlay.addEventListener("click", this._closeCommandBar.bind(this), { once: true });
    const commandBarCancelButton = templateFragment.getElementById("commandBarCancelButton");
          commandBarCancelButton.addEventListener("click", this._closeCommandBar.bind(this), { once: true });

    return templateFragment;
  }


  /**
   * Constructs the commands panel and adds all the required event handlers
   * Returns the root html element (commandsPanel)
   **/
  async _buildCommandsPanel () {
    const templateFragment = document.createRange().createContextualFragment(`
      <div id="commandsPanel" class="cb-panel">
        <div class="cb-head">
          <div id="commandsHeading" class="cb-heading"></div>
          <button id="commandsSearchButton" type="button"></button>
          <input id="commandsSearchInput" class="input-field">
        </div>
        <div id="commandsMain" class="cb-main">
          <div id="commandsScrollContainer" class="cb-scroll-container"></div>
        </div>
      </div>
    `);

    const commandsHeading = templateFragment.getElementById("commandsHeading");
          commandsHeading.title = commandsHeading.textContent = browser.i18n.getMessage('commandBarTitle');

    const commandsSearchButton = templateFragment.getElementById("commandsSearchButton");
          commandsSearchButton.onclick = this._handleSearchButtonClick.bind(this);

    const commandsSearchInput = templateFragment.getElementById('commandsSearchInput');
          commandsSearchInput.oninput = this._handleSearchInput.bind(this);
          commandsSearchInput.placeholder = browser.i18n.getMessage('commandBarSearch');

    const commandsScrollContainer = templateFragment.getElementById("commandsScrollContainer");

    // build command list
    const groups = new Map();

    for (let commandItem of await COMMAND_ITEMS) {
      const item = document.createElement("li");
            item.classList.add("cb-command-item");
            item.dataset.command = commandItem.command;
            item.onclick = this._handleCommandItemClick.bind(this);
            item.onmouseleave = this._handleCommandItemMouseLeave.bind(this);
            item.onmouseenter = this._handleCommandItemMouseOver.bind(this);
      const itemContainer = document.createElement("div");
            itemContainer.classList.add("cb-command-container");
      const label = document.createElement("span");
            label.classList.add("cb-command-name");
            label.textContent = browser.i18n.getMessage(`commandLabel${commandItem.command}`);

      itemContainer.append(label);

      // add settings symbol and build info string
      if (commandItem.settings) {
        const icon = document.createElement("span");
              icon.classList.add("cb-command-settings-icon");
              icon.title = browser.i18n.getMessage("commandBarAdditionalSettingsText");

        itemContainer.append(icon);
      }

      const info = document.createElement("div");
            info.classList.add("cb-command-info");
      const description = document.createElement("span");
            description.classList.add("cb-command-description");
            description.textContent = browser.i18n.getMessage(`commandDescription${commandItem.command}`);

      info.append(description);

      // add permissions symbol and build info string
      if (commandItem.permissions) {
        const icon = document.createElement("span");
              icon.classList.add("cb-command-permissions-icon");
              icon.title = browser.i18n.getMessage("commandBarAdditionalPermissionsText");

        commandItem.permissions.forEach((permission, index) => {
          if (index > 0) icon.title += ", ";
          icon.title += browser.i18n.getMessage(`permissionLabel${permission}`);
        });

        info.append(icon);
      }

      item.append(itemContainer, info);

      if (groups.has(commandItem.group)) {
        const list = groups.get(commandItem.group);
        list.appendChild(item);
      }
      else {
        const list = document.createElement("ul");
              list.classList.add("cb-command-group");
        groups.set(commandItem.group, list);
        list.appendChild(item);
        commandsScrollContainer.appendChild(list);
      }
    }

    // mark the currently active command item
    if (this.command instanceof Command) {
      const currentCommandItem = templateFragment.querySelector(`.cb-command-item[data-command=${this.command.getName()}]`);
      currentCommandItem.classList.add("cb-active");
    }
    // return html panel element
    return templateFragment.firstElementChild;
  }


  /**
   * Constructs the settings panel of the currently selected command and adds all the required event handlers
   * Returns the root html element (settingsPanel)
   **/
  async _buildSettingsPanel () {
    const templateFragment = document.createRange().createContextualFragment(`
      <div id="settingsPanel" class="cb-panel">
        <div class="cb-head">
          <button id="settingsBackButton" type="button"></button>
          <div id="settingsHeading" class="cb-heading"></div>
        </div>
        <div id="settingsMain" class="cb-main">
          <div id="settingsScrollContainer" class="cb-scroll-container"></div>
        </div>
      </div>
    `);

    // register event handlers
    const settingsBackButton = templateFragment.getElementById("settingsBackButton");
          settingsBackButton.onclick = this._handleBackButtonClick.bind(this);

    const settingsScrollContainer = templateFragment.getElementById("settingsScrollContainer");
    const settingsHeading = templateFragment.getElementById("settingsHeading");

    // set heading
    settingsHeading.title = settingsHeading.textContent = this._selectedCommand.toString();
    // create form
    const settingsForm = document.createElement("form");
          settingsForm.id = "settingsForm";
          settingsForm.onsubmit = this._handleFormSubmit.bind(this);
    // create and apend save button
    const saveButton = document.createElement("button");
          saveButton.id = "settingsSaveButton";
          saveButton.type = "submit";
          saveButton.textContent = browser.i18n.getMessage('buttonSave');
          settingsForm.appendChild(saveButton);

    // get the corresponding setting templates
    const settingTemplates = (await COMMAND_SETTING_TEMPLATES).querySelectorAll(
      `[data-commands~="${this._selectedCommand.getName()}"]`
    );

     // build and insert settings
    for (const settingTemplate of settingTemplates) {
      const settingContainer = document.createElement("div");
            settingContainer.classList.add("cb-setting");
      // call importNode (instead of cloneNode) so nodes get the root document as the owner document
      // this is important for custom elements, because they are defined in the root document
      const setting = document.importNode(settingTemplate.content, true);
      // append the current settings
      settingContainer.appendChild(setting);
      settingsForm.insertBefore(settingContainer, saveButton);
    }

    // insert text from language files
    for (let element of settingsForm.querySelectorAll('[data-i18n]')) {
      element.textContent = browser.i18n.getMessage(element.dataset.i18n);
    }

    // insert command settings
    for (let settingInput of settingsForm.querySelectorAll("[name]")) {
      let value;
      // if the currently active command is selected get the last chosen command setting
      if (this.command instanceof Command && this.command.getName() === this._selectedCommand.getName() && this.command.hasSetting(settingInput.name)) {
        value = this.command.getSetting(settingInput.name);
      }
      else {
        value = this._selectedCommand.getSetting(settingInput.name);
      }

      if (settingInput.type === "checkbox") settingInput.checked = value;
      else settingInput.value = value;
    }

    // append form
    settingsScrollContainer.appendChild(settingsForm);
    // return html panel element
    return templateFragment.firstElementChild;
  }


  /**
   * Opens the command bar and shows the commands panel
   **/
  _openCommandBar (panel) {
    // ignore if command bar is already open
    if (this.shadowRoot.getElementById("commandBar")) return;

    const commandBarFragment = this._buildCommandBar();

    const commandBarWrapper = commandBarFragment.getElementById("commandBarWrapper");
    // add commands panel
    commandBarWrapper.appendChild(panel);

    const overlay = commandBarFragment.getElementById("overlay");
    const commandBar = commandBarFragment.getElementById("commandBar");

    overlay.classList.add("o-hide");
    commandBar.classList.add("cb-hide");

    // append to shadow dom
    this.shadowRoot.appendChild( commandBarFragment );

    commandBar.offsetHeight;

    overlay.classList.replace("o-hide", "o-show");
    commandBar.classList.replace("cb-hide", "cb-show");
  }


  /**
   * Closes the command bar and resets the internal variables
   **/
  _closeCommandBar () {
    const overlay = this.shadowRoot.getElementById("overlay");
    const commandBar = this.shadowRoot.getElementById("commandBar");

    overlay.addEventListener("transitionend", function removeOverlay(event) {
      // prevent the event from firing for child transitions
      if (event.currentTarget === event.target) {
        overlay.removeEventListener("transitionend", removeOverlay);
        overlay.remove();
      }
    });
    commandBar.addEventListener("transitionend", function removeCommandBar(event) {
      // prevent the event from firing for child transitions
      if (event.currentTarget === event.target) {
        commandBar.removeEventListener("transitionend", removeCommandBar);
        commandBar.remove();
      }
    });

    overlay.classList.replace("o-show", "o-hide");
    commandBar.classList.replace("cb-show", "cb-hide");

    // reset variables
    this._selectedCommand = null;
    this._scrollPosition = 0;
  }


  /**
   * Hides the settings panel and switches to the commands panel
   **/
  _showNewPanel (newPanel, slideInTransitionClass, slideOutTransitionClass) {
    const commandBarWrapper = this.shadowRoot.getElementById("commandBarWrapper");
    const currentPanel = this.shadowRoot.querySelector(".cb-panel");

    currentPanel.classList.add("cb-init-slide");
    newPanel.classList.add("cb-init-slide", slideInTransitionClass);

    currentPanel.addEventListener("transitionend", function removePreviousPanel(event) {
      // prevent event bubbeling
      if (event.currentTarget === event.target) {
        currentPanel.remove();
        currentPanel.classList.remove("cb-init-slide", slideOutTransitionClass);
        currentPanel.removeEventListener("transitionend", removePreviousPanel);
      }
    });
    newPanel.addEventListener("transitionend", function finishNewPanel(event) {
      // prevent event bubbeling
      if (event.currentTarget === event.target) {
        newPanel.classList.remove("cb-init-slide");
        newPanel.removeEventListener("transitionend", finishNewPanel);
      }
    });

    commandBarWrapper.appendChild(newPanel);
    // trigger reflow
    commandBarWrapper.offsetHeight;

    currentPanel.classList.add(slideOutTransitionClass);
    newPanel.classList.remove(slideInTransitionClass);
  }


  /**
   * Opens the command bar if it isn't already open and the necessary data is available
   **/
  async _handleMainButtonClick (event) {
    this._openCommandBar( await this._buildCommandsPanel() );
  }


  /**
   * Opens the command bar if it isn't already open and the necessary data is available
   **/
  async _handleSecondaryButtonClick (event) {
    if (this.command?.hasSettings()) {
      this._selectedCommand = this.command;
      this._openCommandBar( await this._buildSettingsPanel() );
    }
    else {
      this._openCommandBar( await this._buildCommandsPanel() );
    }
  }

  /**
   * Shows the description of the current command if the command is hovered for 0.5 seconds
   **/
  _handleCommandItemMouseOver (event) {
    const commandItem = event.currentTarget;
    setTimeout(() => {
      if (commandItem.matches(".cb-command-item:hover")) {
        const commandItemInfo = commandItem.querySelector(".cb-command-info");
        if (!commandItemInfo.style.getPropertyValue("height")) {
          commandItemInfo.style.setProperty("height", commandItemInfo.scrollHeight + "px");
        }
      }
    }, 500);
  }


  /**
   * Hides the describtion of the current command
   **/
  _handleCommandItemMouseLeave (event) {
    const commandItemInfo = event.currentTarget.querySelector(".cb-command-info");
    if (commandItemInfo.style.getPropertyValue("height")) {
      commandItemInfo.style.removeProperty("height");
    }
  }


  /**
   * Handles the command selection procedure
   * Asks the user for extra permissions if required
   * Switches to the command settings if existing
   **/
  async _handleCommandItemClick (event) {
    // hide command description
    const commandItemInfo = event.currentTarget.querySelector('.cb-command-info');
          commandItemInfo.style.removeProperty("height");

    // get command item
    const commandItem = (await COMMAND_ITEMS).find((element) => {
      return element.command === event.currentTarget.dataset.command;
    });

    // if the command requires permissions
    if (commandItem.permissions) {
      const permissionRequest = browser.permissions.request({
        permissions: commandItem.permissions,
      });
      // exit if permissions aren't granted
      if (!await permissionRequest) return;
    }

    // if the command offers any settings show them
    if (commandItem.settings) {
      const commandsMain = this.shadowRoot.getElementById("commandsMain");
      // store current scroll position
      this._scrollPosition = commandsMain.scrollTop;
      // store a reference to the selected command
      this._selectedCommand = new Command(commandItem.command, commandItem.settings);

      const settingsPanel = await this._buildSettingsPanel();
      this._showNewPanel(settingsPanel, "cb-slide-right", "cb-slide-left");
    }
    else {
      this.command = new Command(commandItem.command);
      this.dispatchEvent( new InputEvent('change') );
      this._closeCommandBar();
    }
  }


  /**
   * Toggles the search input
   **/
  _handleSearchButtonClick () {
    const commandsPanel = this.shadowRoot.getElementById('commandsPanel');
    const input = this.shadowRoot.getElementById('commandsSearchInput');

    // after hiding the searchbar, the search is cleared and the bar is reset.
    if (!commandsPanel.classList.toggle('search-visible')) {
      input.value = "";
      this._handleSearchInput();
    }
    else {
      input.focus();
    }
  }


  /**
   * Show or hide the searched results in the command bar.
   **/
  _handleSearchInput () {
    const commandsPanel = this.shadowRoot.getElementById('commandsPanel');
    const searchQuery = this.shadowRoot.getElementById('commandsSearchInput').value.toLowerCase().trim();
    const searchQueryKeywords = searchQuery.split(" ");

    // if the search query is not empty hide all groups to remove the padding and border
    commandsPanel.classList.toggle('search-runs', searchQuery);

    const commandNameElements = this.shadowRoot.querySelectorAll('.cb-command-name');
    for (let commandNameElement of commandNameElements) {
      const commandName = commandNameElement.textContent.toLowerCase().trim();
      // check if all keywords are matching the command name
      const isMatching = searchQueryKeywords.every(keyword => commandName.includes(keyword));
      // hide all unmatching commands and show all matching commands
      commandNameElement.closest('.cb-command-item').hidden = !isMatching;
    }
  }


  /**
   * Hides the settings panel and shows the command panel
   **/
  async _handleBackButtonClick () {
    // build panel with the last scroll position
    const commandsPanel = await this._buildCommandsPanel();
    this._showNewPanel(commandsPanel, "cb-slide-left", "cb-slide-right");

    // reapply stored scroll position
    // scroll position needs to be set after element is appended to DOM
    // because the height of the element is unknown if not rendered
    const commandsMain = this.shadowRoot.getElementById("commandsMain");
    commandsMain.scrollTop = this._scrollPosition;
  }


  /**
   * Gathers and saves the specified settings data from the all input elements and closes the command bar
   **/
  _handleFormSubmit (event) {
    // prevent page reload
    event.preventDefault();

    const settingInputs = this.shadowRoot.querySelectorAll("#settingsForm [name]");
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
      this._selectedCommand.setSetting(settingInput.name, value);
    }

    this.command = this._selectedCommand;
    this.dispatchEvent( new InputEvent('change') );
    this._closeCommandBar();
  }
}

// define custom element <command-select></command-select>
window.customElements.define('command-select', CommandSelect);