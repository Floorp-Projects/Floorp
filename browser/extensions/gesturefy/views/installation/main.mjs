// insert text from language files
for (let element of document.querySelectorAll('[data-i18n]')) {
  element.textContent = browser.i18n.getMessage(element.dataset.i18n);
}