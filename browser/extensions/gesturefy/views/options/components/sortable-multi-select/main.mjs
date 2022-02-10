// getter for module path
const MODULE_DIR = (() => {
  const urlPath = new URL(import.meta.url).pathname;
  return urlPath.slice(0, urlPath.lastIndexOf("/") + 1);
})();


/**
 * Main HTML document fragment
 * This serves as a template for new elements and will be appended to the shadow DOM
 * Global declaration so it is only parsed once
 **/
const template = document.createRange().createContextualFragment(`
  <link rel="stylesheet" href="${MODULE_DIR}layout.css">
  <div id="wrapper">
    <div id="selection" part="selection" tabindex="-1" data-event-handler-namespace="selection">
      <div id="items"></div>
      <input id="search" part="search" placeholder="Add" data-event-handler-namespace="search">
    </div>
    <slot id="dropdown" part="dropdown" class="hidden" tabindex="-1" data-placeholder="No results" data-event-handler-namespace="dropdown"></slot>
  </div>
`);


/**
 * Custom element - <sortable-multi-select>
 * Only <sortable-multi-select-item> are allowed as child elements
 * Items can be rearranged via drag and drop
 * Accepts 4 special attributes
 * value : Array of strings/values where each must match one <sortable-multi-select-item> value
 * name : similar to a select field
 * placeholder : similar to an input field
 * dropdown-placeholder : this text will be shown in the dropdown if no item was found
 * Dispatches a "change" event on value changes
 **/
export class SortableMultiSelect extends HTMLElement {

  /**
   * Construcor
   * Set default variables
   * Create shadow root, load stylesheet and html by appending it to the shadow DOM
   * Add all required event listeners
   **/
  constructor() {
    super();

    this.attachShadow({ mode: 'open', delegatesFocus: true }).append(
      document.importNode(template, true)
    );

    // use WeakMap so removed DOM elements (without reference) will automatically be removed from the list when needed
    this._itemBBoxMap = new WeakMap();
    this._draggedItem = null;
    this._draggedItemOriginalSuccessor = null;

    // update box dimensions on change
    this._resizeObserver = new ResizeObserver(entries => {
      for (const entry of entries) {
        if (entry.borderBoxSize) {
          // checking for chrome as using a non-standard array
          if (entry.borderBoxSize[0]) {
            entry.target.style.setProperty("--bboxHeight", entry.borderBoxSize[0].blockSize);
            entry.target.style.setProperty("--bboxWidth", entry.borderBoxSize[0].inlineSize);
          }
          else {
            entry.target.style.setProperty("--bboxHeight", entry.borderBoxSize.blockSize);
            entry.target.style.setProperty("--bboxWidth", entry.borderBoxSize.inlineSize);
          }
        }
      }
    });

    this.addEventListener('dragleave', this);

    const wrapper = this.shadowRoot.getElementById("wrapper");
    wrapper.addEventListener('focusout', this);

    const selection = this.shadowRoot.getElementById("selection");
    selection.addEventListener("focus", this);
    selection.addEventListener("dragover", this);

    const items = this.shadowRoot.getElementById("items");
    const mutationObserver = new MutationObserver(entries => {
      const items = entries.pop().target.children;
      // update all item bboxes on change
      for (const item of items) {
        this._itemBBoxMap.set(item, item.getBoundingClientRect());
      }
    });
    mutationObserver.observe(items, { childList: true, subtree: true });

    const search = this.shadowRoot.getElementById("search");
    search.addEventListener("input", this);
    search.addEventListener("focus", this);
    search.addEventListener('keydown', this);

    const dropdown = this.shadowRoot.getElementById("dropdown");
    dropdown.addEventListener("click", this);
    dropdown.addEventListener('keydown', this);
    dropdown.addEventListener("animationend", this);
    dropdown.addEventListener("slotchange", this);
  }


  /**
   * Add attribute observer
   **/
  static get observedAttributes() {
    return ["placeholder", "dropdown-placeholder"];
  }


  /**
   * Attribute change handler
   * Update placholder texts on changes
   **/
  attributeChangedCallback (name, oldValue, newValue) {
    switch (name) {
      case "placeholder":
        const search = this.shadowRoot.getElementById("search");
        search.placeholder = newValue;
      break;
      case "dropdown-placeholder":
        const dropdown = this.shadowRoot.getElementById("dropdown");
        dropdown.dataset.placeholder = newValue;
      break;
    }
  }


  /**
   * Getter for the "value" attribute
   * Returns an array of strings
   **/
  get value () {
    const items = this.shadowRoot.getElementById("items");
    return Array.from(items.children).map((item) => item.dataset.value);
  }


  /**
   * Getter for the "value" attribute
   * Accepts array and stringified array
   **/
  set value (values) {
    // if string try to parse as JSON
    if (typeof values === "string") {
      values = JSON.parse(values);
    }
    // only set value if is array
    if (Array.isArray(values)) {
      // remove all previous items
      const items = this.shadowRoot.getElementById("items");
      while (items.firstChild) items.firstChild.remove();

      const dropdown = this.shadowRoot.getElementById("dropdown");
      const dropdownItems = dropdown.assignedElements();

      // add new items
      for (let itemValue of values) {
        // get corresponding dropdown item by value
        const dropdownItem = dropdownItems.find(item => item.value === itemValue);
        if (dropdownItem) {
          // create item and add it to the selection
          const newItem = this._buildSelectedItem(dropdownItem.value, dropdownItem.label);
          items.append(newItem);
        }
      }
    }
  }


  /**
   * Getter for the "name" attribute
   **/
  get name () {
    return this.getAttribute("name");
  }


  /**
   * Setter for the "name" attribute
   **/
  set name (value) {
    this.setAttribute("name", value);
  }


  /**
   * Getter for the "placeholder" attribute
   **/
  get placeholder () {
    return this.getAttribute("placeholder");
  }


  /**
   * Setter for the "placeholder" attribute
   **/
  set placeholder (value) {
    this.setAttribute("placeholder", value);
  }


  /**
   * Getter for the "placeholder-placeholder" attribute
   **/
   get dropdownPlaceholder () {
    return this.getAttribute("dropdown-placeholder");
  }


  /**
   * Setter for the "placeholder-placeholder" attribute
   **/
  set dropdownPlaceholder (value) {
    this.setAttribute("dropdown-placeholder", value);
  }


  /**
   * This function handles all events from this class and delegates them to their specific functions
   * Every function has to closely follow the following naming convention:
   * _handle[capitalized optional event handler namespace][capitalized event name]
   * The event handler namespace can be specified on the target element with the datat attribute:
   * event-handler-namespace = "name_here"
   * Example: _handleSelectionClick or without namespace _handleClick
   **/
  handleEvent (event) {
    const namespace = event.currentTarget.dataset.eventHandlerNamespace;
    const type = event.type;
    // construct event handler name
    let handlerName = '_handle';
    if (namespace) {
      handlerName += namespace[0].toUpperCase() + namespace.slice(1);
    }
    handlerName += type[0].toUpperCase() + type.slice(1);
    // run corresponding handler
    if (typeof this[handlerName] === "function") {
      this[handlerName](event);
    }
    else {
      throw(`"${handlerName}" is not a valid function name.`);
    }
  }


  /**
   * Constructs an item shown in the selection by a given value and label
   * Returns the created item
   **/
  _buildSelectedItem (itemValue, itemLabel) {
    const item = document.createElement("div");
    item.part.add("item");
    item.classList.add("item");
    item.draggable = true;
    item.tabIndex = 0;
    item.dataset.value = itemValue;
    item.dataset.eventHandlerNamespace = "item";
    item.addEventListener("dragstart", this);
    item.addEventListener("dragend", this);
    item.addEventListener("animationend", this);

    const itemContent = document.createElement("span");
    itemContent.part.add("item-inner-text");
    itemContent.classList.add("item-inner-text");
    itemContent.textContent = itemLabel;

    const itemRemoveButton = document.createElement("button");
    itemRemoveButton.part.add("item-remove-button");
    itemRemoveButton.classList.add("item-remove-button");
    itemRemoveButton.dataset.eventHandlerNamespace = "itemRemoveButton";
    itemRemoveButton.addEventListener("click", this);

    item.append(itemContent, itemRemoveButton);
    return item;
  }


  /**
   * Set focus to the search/filter input field
   **/
  _focusSearch () {
    const search = this.shadowRoot.getElementById('search');
    search.focus();
  }


  /**
   * Clear the search/filter input field
   **/
  _clearSearch () {
    const search = this.shadowRoot.getElementById('search');
    search.value = "";
  }


  /**
   * Opens the dropdown if it isn't already open
   * Calculates some relevant positional values for the dropdown and registers the resize observer
   **/
  _showDropdown () {
    const dropdown = this.shadowRoot.getElementById("dropdown");
    if (dropdown.classList.contains("hidden")) {
      const wrapper = this.shadowRoot.getElementById("wrapper");
      this._resizeObserver.observe(wrapper, { box: "border-box" });

      const bbox = wrapper.getBoundingClientRect();
      wrapper.style.setProperty("--bboxX", bbox.x);
      wrapper.style.setProperty("--bboxY", bbox.y);

      const spaceTop = bbox.y;
      const spaceBottom = window.innerHeight - (bbox.y + bbox.height);

      // get the absolute maximum available height from the current position either from the top or bottom
      dropdown.classList.toggle("shift", spaceBottom < spaceTop);
      dropdown.classList.remove("hidden");
      dropdown.classList.add("animate");
    }
  }


  /**
   * Filters the dropdown items by a given string
   * Items that do not match the filter keywords will be hidden
   * If no item matches the dropdown-placeholder will be shown
   **/
  _filterDropdown (filterString) {
    const dropdown = this.shadowRoot.getElementById("dropdown");
    const filterStringKeywords = filterString.toLowerCase().trim().split(" ");
    let hasResults = false;

    for (const item of dropdown.assignedElements()) {
      const itemContent = item.value.toLowerCase().trim();
      // check if all keywords are matching the command name
      const isMatching = filterStringKeywords.every(keyword => itemContent.includes(keyword));
      // hide all unmatching commands and show all matching commands
      item.hidden = !isMatching;
      if (isMatching) hasResults = true;
    }

    // if no item is visible add indicator class
    dropdown.part.toggle("no-results", !hasResults);
    dropdown.classList.toggle("no-results", !hasResults);
  }


  /**
   * Closes the dropdown if it isn't already closed
   * This also disconnects the resize observer
   **/
  _hideDropdown () {
    const dropdown = this.shadowRoot.getElementById("dropdown");
    if (!dropdown.classList.contains("hidden")) {
      this._resizeObserver.disconnect();

      dropdown.classList.add("hidden", "animate");
    }
  }


  /**
   * Handle focus out of the main element
   * Close the dropdown and clear the search on focus out
   **/
  _handleFocusout () {
    const wrapper = this.shadowRoot.getElementById("wrapper");
    // only hide if no element inside has focus
    if (!wrapper.matches(":focus-within")) {
      this._hideDropdown();
      this._clearSearch();
    }
  }


  /**
   * Forward focus to search input
   **/
  _handleSelectionFocus (event) {
    this._focusSearch();
  }


  /**
   * Check all children in the slot if they are of type <sortable-multi-select-item>
   * This throws an error if an element is of a different type
   **/
  _handleDropdownSlotchange (event) {
    const dropdown = this.shadowRoot.getElementById("dropdown");
    const expectedChildElements = dropdown.assignedElements().every(element => {
      return element instanceof SortableMultiSelectItem;
    });
    if (!expectedChildElements) {
      throw("All sortable-multi-select children should be of type oderable-multi-select-item.");
    }
  }


  /**
   * Handles the item selection from the dropdown
   * This creates a new item and appends it to the selection
   * A "change" event will be dispatched
   **/
  _handleDropdownClick (event) {
    const dropdown = this.shadowRoot.getElementById("dropdown");
    // get closest slotted element
    const item = event.composedPath().find((ele) => ele.assignedSlot === dropdown);
    if (item) {
      // create item and add it to the selection
      const items = this.shadowRoot.getElementById("items");
      const newItem = this._buildSelectedItem(item.value, item.label);
      newItem.classList.add("animate");
      items.append(newItem);
      // dispatch change event
      this.dispatchEvent( new InputEvent('change') );
    }
  }


  /**
   * Handles cleanup on dropdown animations
   * This also resets the dropdown filter on dropdown hide
   **/
  _handleDropdownAnimationend (event) {
    if (event.animationName === "hideDropdown") {
      // show hidden items
      this._filterDropdown("");
    }
    event.currentTarget.classList.remove("animate");
  }


  /**
   * Handle dropdown keyboard inputs
   * Up and down arrow keys can be used for navigation
   **/
  _handleDropdownKeydown (event) {
    if (event.key === "ArrowUp" || event.key === "ArrowDown") {
      const dropdown = this.shadowRoot.getElementById('dropdown');
      const matchingItems = dropdown.assignedElements().filter(item => {
        return item.hidden !== true;
      });

      if (matchingItems.length === 0) return;

      // if not item is currently focused -1 will be returned
      const focusedItemIndex = matchingItems.findIndex(item => {
        return item === event.target;
      });

      switch (event.key) {
        case "ArrowUp":
          matchingItems[focusedItemIndex - 1]?.focus();
        break;

        case "ArrowDown":
          // if no item is currently selected and arrow down is pressed this will equal 0
          // thus the first item will be focused
          matchingItems[focusedItemIndex + 1]?.focus();
        break;
      }

      event.preventDefault();
    }
  }


  /**
   * Show the dropdown when the search input receives focus
   **/
  _handleSearchFocus () {
    this._showDropdown();
  }


  /**
   * Forwards search input events to the dropdown keyboard handler
   **/
  _handleSearchKeydown (event) {
    this._handleDropdownKeydown(event);
  }


  /**
   * Handles search inputs and filters the dropdown
   **/
  _handleSearchInput () {
    const search = this.shadowRoot.getElementById('search');
    this._filterDropdown(search.value);
  }


  /**
   * Handles cleanup on item animations
   **/
  _handleItemAnimationend (event) {
    event.currentTarget.classList.remove("animate");
  }


  /**
   * Removes the item from the selection
   * Sets focus to the next item if one exists
   * Dispatch "change" event
   **/
  _handleItemRemoveButtonClick (event) {
    const item = event.target.closest(".item");
    // focus next or previous item on removal
    (item.nextElementSibling ?? item.previousElementSibling)?.focus();
    // remove item
    item?.remove();
    // dispatch change event
    this.dispatchEvent( new InputEvent('change') );
  }


  /**
   * Handle item drag start
   * Set necessary variables, manage drag ghost image and change the style of the dragged item
   **/
  _handleItemDragstart (event) {
    this._draggedItem = event.target;
    this._draggedItemOriginalSuccessor = this._draggedItem.nextElementSibling;

    const items = this.shadowRoot.getElementById("items").children;
    // update all item bboxes
    for (const item of items) {
      this._itemBBoxMap.set(item, item.getBoundingClientRect());
    }

    // delay otherwise the drag ghost image is effected
    setTimeout(() => {
      this._draggedItem.part.add("dragged");
      this._draggedItem.classList.add("dragged");
    }, 0);
  }


  /**
   * Handle item drag end/cancel
   * Dispatch "change" event if item order changed
   * Cleanup variables
   **/
  _handleItemDragend (event) {
    // on cancel or no drop location place dragged item back to start
    if (event.dataTransfer.dropEffect === "none") {
      const items = this.shadowRoot.getElementById("items");
      if (this._draggedItemOriginalSuccessor) {
        this._draggedItemOriginalSuccessor.before(this._draggedItem);
      }
      else {
        items.append(this._draggedItem);
      }
    }
    // dispatch change event if the position of the dragged item differs from its previous position
    else if (this._draggedItemOriginalSuccessor !== this._draggedItem.nextElementSibling) {
      this.dispatchEvent( new InputEvent('change') );
    }
    this._draggedItem.part.remove("dragged");
    this._draggedItem.classList.remove("dragged");
    this._draggedItem.classList.add("animate");
    this._draggedItem = null;
    this._draggedItemOriginalSuccessor = null;
  }


  /**
   * Handle selection dragover event for items
   * This calculates and updates the position/order of the dragged item
   **/
  _handleSelectionDragover (event) {
    // ignore foreign drag items
    if (!this._draggedItem) return;

    const items = this.shadowRoot.getElementById("items").children;
    let minDistance = Infinity;
    let nearestItem = null;

    for (const item of items) {
      const bbox = this._itemBBoxMap.get(item);
      // if cursor is between item rows ignore item
      if (event.clientY < bbox.y || event.clientY > (bbox.y + bbox.height)) {
        continue;
      }
      // calc distance to item center
      const dx = event.clientX - (bbox.x + bbox.width / 2);
      const dy = event.clientY - (bbox.y + bbox.height / 2);
      const distance = Math.hypot(dx, dy);
      // save reference to nearest element if distance is lower than minDistance
      if (distance < minDistance) {
        minDistance = distance;
        nearestItem = item;
      }
    }

    // if nearest item is not dragged item
    if (nearestItem && nearestItem !== this._draggedItem) {
      const draggedItemBBox = this._itemBBoxMap.get(this._draggedItem);
      const nearestItemBBox = this._itemBBoxMap.get(nearestItem);
      const itemCenterX = nearestItemBBox.x + nearestItemBBox.width / 2;
      const itemCenterY = nearestItemBBox.y + nearestItemBBox.height / 2;

      const wrapper = this.shadowRoot.getElementById("wrapper");
      const containerWidth = Number(wrapper.style.getPropertyValue("--bboxWidth"));

      // if there is not enough space for the dragged item look at the y axis
      // this is no ideal, but helps to deal with items that span the whole width
      if (containerWidth - nearestItemBBox.width < draggedItemBBox.width) {
        if (itemCenterY > event.clientY) {
          if (nearestItem.previousElementSibling !== this._draggedItem) {
            nearestItem.before(this._draggedItem);
          }
        }
        else if (nearestItem.nextElementSibling !== this._draggedItem) {
          nearestItem.after(this._draggedItem);
        }
      }
      else {
        if (itemCenterX > event.clientX) {
          if (nearestItem.previousElementSibling !== this._draggedItem) {
            nearestItem.before(this._draggedItem);
          }
        }
        else if (nearestItem.nextElementSibling !== this._draggedItem) {
          nearestItem.after(this._draggedItem);
        }
      }
    }
    // if dragged item is placed
    if (this._draggedItem?.isConnected) {
      // prevent ghost drag element slide back/revert effect
      // and indicate move operation
      event.preventDefault();
    }
  }


  /**
   * Remove the item from the selection if its dragged outside of the element
   **/
  _handleDragleave (event) {
    this._draggedItem?.remove();
  }
}

// define custom element <sortable-multi-select></sortable-multi-select>
window.customElements.define('sortable-multi-select', SortableMultiSelect);


/**
 * Custom element - <sortable-multi-select-item>
 * Accepts 3 special attributes (and properties):
 * value : should be a unique string
 * label : the label that will be shown in the selection
 * disabled : prevents the item from beeing selected
 **/
 export class SortableMultiSelectItem extends HTMLElement {

  /**
   * Construcor
   * Add required event listeners
   **/
  constructor() {
    super();

    this.addEventListener("keyup", this._handleKeyup);
    this.addEventListener("click", this._handleClick);
  }


  /**
   * Attribute change handler
   * Set tabIndex so the element can be focused
   **/
  connectedCallback() {
    // add tabindex on connected callback otherwise it has no effect
    // -1 because the element should only be focusable by code
    if (!this.hasAttribute('tabindex')) {
      this.tabIndex = -1;
    }
  }


  /**
   * Getter for the "value" attribute
   * If missing this will return the textContent of the element
   **/
  get value () {
    return this.getAttribute("value") || this.textContent.trim();
  }


  /**
   * Setter for the "value" attribute
   **/
  set value (value) {
    this.setAttribute("value", value);
  }


  /**
   * Getter for the "label" attribute
   * If missing this will return the textContent of the element
   **/
  get label () {
    return this.getAttribute("label") || this.textContent.trim();
  }


  /**
   * Setter for the "label" attribute
   **/
  set label (value) {
    this.setAttribute("label", value);
  }


  /**
   * Getter for the "disabled" attribute
   **/
  get disabled () {
    return this.hasAttribute("disabled");
  }


  /**
   * Setter for the "disabled" attribute
   **/
  set disabled (value) {
    this.toggleAttribute("disabled", Boolean(value));
  }


  /**
   * Forward enter key as click event
   **/
  _handleKeyup (event) {
    if (event.key === "Enter" && !this.disabled) {
      this.click(event);
    }
  }

  /**
   * Prevent click events if element is disabled
   **/
  _handleClick (event) {
    if (this.disabled) {
      event.stopImmediatePropagation();
      event.preventDefault();
    }
  }
}

// define custom element <sortable-multi-select-item></sortable-multi-select-item>
window.customElements.define('sortable-multi-select-item', SortableMultiSelectItem);