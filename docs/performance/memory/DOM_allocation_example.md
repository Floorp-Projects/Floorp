# DOM allocation example

This article describes a very simple web page that we\'ll use to
illustrate some features of the Memory tool.

You can try out the site at
<https://mdn.github.io/performance-scenarios/dom-allocs/alloc.html>.

It just contains a script that creates a large number of DOM nodes:

``` {.brush: .js}
var toolbarButtonCount = 20;
var toolbarCount = 200;

function getRandomInt(min, max) {
    return Math.floor(Math.random() * (max - min + 1)) + min;
}

function createToolbarButton() {
  var toolbarButton = document.createElement("span");
  toolbarButton.classList.add("toolbarbutton");
  // stop Spidermonkey from sharing instances
  toolbarButton[getRandomInt(0,5000)] = "foo";
  return toolbarButton;
}

function createToolbar() {
  var toolbar = document.createElement("div");
  // stop Spidermonkey from sharing instances
  toolbar[getRandomInt(0,5000)] = "foo";
  for (var i = 0; i < toolbarButtonCount; i++) {
    var toolbarButton = createToolbarButton();
    toolbar.appendChild(toolbarButton);
  }
  return toolbar;
}

function createToolbars() {
  var container = document.getElementById("container");
  for (var i = 0; i < toolbarCount; i++) {
    var toolbar = createToolbar();
    container.appendChild(toolbar);
  }
}

createToolbars();
```

A simple pseudocode representation of how this code operates looks like
this:

    createToolbars()
        -> createToolbar() // called 200 times, creates 1 DIV element each time
           -> createToolbarButton() // called 20 times per toolbar, creates 1 SPAN element each time

In total, then, it creates 200 `HTMLDivElement` objects, and 4000
`HTMLSpanElement` objects.
