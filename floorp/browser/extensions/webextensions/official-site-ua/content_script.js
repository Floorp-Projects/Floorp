let window_ = window.wrappedJSObject.window;

if (window_.name.startsWith("webpanel")) {
    let elem = document.createElement("script");
    elem.src = browser.runtime.getURL("inject.js");
    document.documentElement.appendChild(elem);
}
