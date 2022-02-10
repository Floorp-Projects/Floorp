import { ContentLoaded, Config } from "/views/options/main.mjs";

ContentLoaded.then(main);

/**
 * main function
 * run code that depends on async resources
 **/
function main () {
  const exclusionsContainer = document.getElementById('exclusionsContainer');
        exclusionsContainer.dataset.noEntriesHint = browser.i18n.getMessage('exclusionsHintNoEntries');
  const exclusionsForm = document.getElementById('exclusionsForm');
        exclusionsForm.onsubmit = onFormSubmit;
        exclusionsForm.elements.urlPattern.placeholder = browser.i18n.getMessage('exclusionsPlaceholderURL');
        exclusionsForm.elements.urlPattern.title = browser.i18n.getMessage('exclusionsPlaceholderURL');
        exclusionsForm.elements.urlPattern.onchange = onInputChange;
  // add existing exclusions entries
  for (const urlPattern of Config.get("Exclusions")) {
    const exclusionsEntry = createExclusionsEntry(urlPattern);
    exclusionsContainer.appendChild(exclusionsEntry);
  }
}


/**
 * Creates a exclusions entry html element by a given url pattern and returns it
 **/
function createExclusionsEntry (urlPattern) {
  const exclusionsEntry = document.createElement('li');
        exclusionsEntry.classList.add('excl-entry');
        exclusionsEntry.dataset.urlPattern = urlPattern;
        exclusionsEntry.onclick = onEntryClick;
  const inputURLEntry = document.createElement('div');
        inputURLEntry.classList.add('excl-url-pattern');
        inputURLEntry.textContent = urlPattern;
  const deleteButton = document.createElement('button');
        deleteButton.type = "button";
        deleteButton.classList.add('excl-remove-button', 'icon-delete');
  exclusionsEntry.append(inputURLEntry, deleteButton);
  return exclusionsEntry;
}


/**
 * Adds a given exclusions entry element to the exclusions ui
 **/
function addExclusionsEntry (exclusionsEntry) {
  const exclusionsContainer = document.getElementById('exclusionsContainer');
  // append entry, hide it and move it out of flow to calculate its dimensions
  exclusionsContainer.prepend(exclusionsEntry);
  exclusionsEntry.style.setProperty('visibility', 'hidden');
  exclusionsEntry.style.setProperty('position', 'absolute');
  // calculate total entry height
  const computedStyle = window.getComputedStyle(exclusionsEntry);
  const outerHeight = parseInt(computedStyle.marginTop) + exclusionsEntry.offsetHeight + parseInt(computedStyle.marginBottom);

  // move all entries up by one entry including the new one
  for (const node of exclusionsContainer.children) {
    node.style.setProperty('transform', `translateY(-${outerHeight}px)`);
    // remove ongoing transitions if existing
    node.style.removeProperty('transition');
  }
  // show new entry and bring it back to flow, which pushes all elements down by the height of one entry
  exclusionsEntry.style.removeProperty('visibility', 'hidden');
  exclusionsEntry.style.removeProperty('position', 'absolute');

  // trigger reflow
  exclusionsContainer.offsetHeight;

  exclusionsEntry.addEventListener('animationend', (event) => {
    event.currentTarget.classList.remove('excl-entry-animate-add');
  }, {once: true });
  exclusionsEntry.classList.add('excl-entry-animate-add');

  // move all entries down including the new one
  for (const node of exclusionsContainer.children) {
    node.addEventListener('transitionend', (event) => event.currentTarget.style.removeProperty('transition'), {once: true });
    node.style.setProperty('transition', 'transform 0.3s');
    node.style.removeProperty('transform');
  }
}


/**
 * Removes a given exclusions entry element from the exclusions ui
 **/
function removeExclusionsEntry (exclusionsEntry) {
  // calculate total entry height
  const computedStyle = window.getComputedStyle(exclusionsEntry);
  const outerHeight = parseInt(computedStyle.marginTop) + exclusionsEntry.offsetHeight + parseInt(computedStyle.marginBottom);

  let node = exclusionsEntry.nextElementSibling;
  while (node) {
    node.addEventListener('transitionend', (event) => {
      event.currentTarget.style.removeProperty('transition');
      event.currentTarget.style.removeProperty('transform');
    }, {once: true });
    node.style.setProperty('transition', 'transform 0.3s');
    node.style.setProperty('transform', `translateY(-${outerHeight}px)`);
    node = node.nextElementSibling;
  }
  exclusionsEntry.addEventListener('animationend', (event) => event.currentTarget.remove(), {once: true });
  exclusionsEntry.classList.add('excl-entry-animate-remove');
}


/**
 * Handles the url pattern submit event
 * Adds the new url pattern to the config and calls the exclusions entry create function
 **/
function onFormSubmit (event) {
  event.preventDefault();
  // remove spaces and cancel the function if the value is empty
  const urlPattern = this.elements.urlPattern.value.trim();
  if (!urlPattern) return;
  // create and add entry to the exclusions
  const exclusionsEntry = createExclusionsEntry(urlPattern);
  addExclusionsEntry(exclusionsEntry);
  // add new url pattern to the beginning of the array
  const exclusionsArray = Config.get("Exclusions");
        exclusionsArray.unshift(urlPattern);
  Config.set("Exclusions", exclusionsArray);
  // clear input field
  this.elements.urlPattern.value = '';
}


/**
 * Handles the url pattern input changes
 * Marks the field as invalide if the entry already exists
 **/
function onInputChange () {
  if (Config.get("Exclusions").indexOf(this.value.trim()) !== -1) {
    this.setCustomValidity(browser.i18n.getMessage('exclusionsNotificationAlreadyExists'));
  }
  else if (this.validity.customError) this.setCustomValidity('');
}


/**
 * Handles the exclusions entry click
 * Calls the remove exclusions entry function on remove button click and removes it from the config
 **/
function onEntryClick (event) {
  // if delete button received the click
  if (event.target.classList.contains('excl-remove-button')) {
    removeExclusionsEntry(this);

    const exclusionsForm = document.getElementById('exclusionsForm');
    // remove input field invaldility if it was previously a duplicate
    if (this.dataset.urlPattern === exclusionsForm.elements.urlPattern.value.trim()) {
      exclusionsForm.elements.urlPattern.setCustomValidity('');
    }
    const exclusionsArray = Config.get("Exclusions");
    // remove url pattern from array
    const index = exclusionsArray.indexOf(this.dataset.urlPattern);
    if (index !== -1) {
      exclusionsArray.splice(index, 1);
      Config.set("Exclusions", exclusionsArray);
    }
  }
}
