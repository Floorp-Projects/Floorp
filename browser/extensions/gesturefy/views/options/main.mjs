import { fetchHTMLAsFragment } from "/core/utils/commons.mjs";

import ConfigManager from "/core/helpers/config-manager.mjs";

export const Config = new ConfigManager("local", browser.runtime.getURL("/resources/json/defaults.json"));


const Resources = [ Config.loaded ];

// load and insert external html fragments
for (let element of document.querySelectorAll('[data-include]')) {
  const fetchingHTML = fetchHTMLAsFragment(browser.runtime.getURL(element.dataset.include));
        fetchingHTML.then((fragment) => element.appendChild(fragment));
  // add to resources
  Resources.push(fetchingHTML);
}

export const ContentLoaded = Promise.all(Resources);

ContentLoaded.then(main);

/**
 * main function
 * run code that depends on async resources
 **/
function main () {
  // insert text from manifest
  const manifest = browser.runtime.getManifest();
  for (const element of document.querySelectorAll('[data-manifest]')) {
    element.textContent = manifest[element.dataset.manifest];
  }

  // insert text from language files
  for (const element of document.querySelectorAll('[data-i18n]')) {
    element.textContent = browser.i18n.getMessage(element.dataset.i18n);
  }

  // apply values to input fields and add their event function
  for (const input of document.querySelectorAll("[data-config]")) {
    const value = Config.get(input.dataset.config);
    if (input.type === "checkbox") {
      input.checked = value;
    }
    else if (input.type === "radio") {
      input.checked = input.value === value;
    }
    else input.value = value;
    input.addEventListener('change', onChage);
  }

  // toggle collapsables and add their event function
  for (const collapse of document.querySelectorAll("[data-collapse]")) {
    collapse.addEventListener('change', onCollapse);
    onCollapse.call(collapse);
  }

  // apply onchange handler and add title to every theme button
  for (const themeButton of document.querySelectorAll('#themeSwitch .theme-button')) {
    themeButton.onchange = onThemeButtonChange;
    themeButton.title = browser.i18n.getMessage(`${themeButton.value}Theme`);
  }
  // apply theme class
  const themeValue = Config.get("Settings.General.theme");
  document.documentElement.classList.add(`${themeValue}-theme`);

  // set default page if not specified and trigger page navigation handler
  window.addEventListener("hashchange", onPageNavigation, true);
  if (!window.location.hash) location.replace('#Gestures');
  else onPageNavigation();

  // set loaded class and render everything
  document.documentElement.classList.add("loaded");
}


/**
 * on hash change / page navigation
 * updates the document title and navbar
 **/
function onPageNavigation () {
  // update the navbar entries highlighting
  const activeItem = document.querySelector('#Sidebar .navigation .nav-item > a.active');
  const nextItem = document.querySelector('#Sidebar .navigation .nav-item > a[href="'+ window.location.hash +'"]');

  if (activeItem) {
    activeItem.classList.remove("active");
  }
  if (nextItem) {
    nextItem.classList.add("active");
    // update document title
    const sectionKey = nextItem.querySelector("[data-i18n]").dataset.i18n;
    document.title = `Gesturefy - ${browser.i18n.getMessage(sectionKey)}`;
  }
}


/**
 * on theme/radio button change
 * store the new theme value
 **/
function onThemeButtonChange () {
  // remove current theme class if any
  document.documentElement.classList.forEach((className) => {
    if (className.endsWith('-theme')) {
      document.documentElement.classList.remove(className);
    }
  });
  // apply theme class + transition class
  document.documentElement.classList.add(`${this.value}-theme`, "theme-transition");
  // remove temporary transition
  window.setTimeout(() => {
    document.documentElement.classList.remove("theme-transition");
  }, 400);
}


/**
 * save input value if valid
 **/
 function onChage () {
  // check if valid, if there is no validity property check if value is set
  if ((this.validity && this.validity.valid) || (!this.validity && this.value)) {
    let value;
    // get true or false for checkboxes
    if (this.type === "checkbox") value = this.checked;
    // get value either as string or number
    else value = isNaN(this.valueAsNumber) ? this.value : this.valueAsNumber;
    // save to config
    Config.set(this.dataset.config, value);
  }
}


/**
 * hide or show on collapse toggle
 **/
function onCollapse (event) {
  const targetElements = document.querySelectorAll(this.dataset["collapse"]);

  for (let element of targetElements) {
    // if user dispatched the function, then hide with animation, else hide without animation
    if (event) {
      element.addEventListener("transitionend", (event) => {
        event.currentTarget.classList.remove("animate");
      }, { once: true });
      element.classList.add("animate");

      if (!this.checked) {
        element.style.height = element.scrollHeight + "px";
        // trigger reflow
        element.offsetHeight;
      }
    }

    if (element.style.height === "0px" && this.checked) {
      element.style.height = element.scrollHeight + "px";
    }
    else if (!this.checked) {
      element.style.height = "0px";
    }
  }
}
