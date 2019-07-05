"use strict";

extensions.registerModules({
  menusChild: {
    url: "chrome://browser/content/child/ext-menus-child.js",
    scopes: ["content_child"],
    paths: [["menus"]],
  },
});
