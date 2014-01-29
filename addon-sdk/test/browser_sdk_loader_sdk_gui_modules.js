function test () {
  // Load `constructor` as global since tabs uses `traits`
  // that use this module
  let loader = makeLoader({ globals: constructor });
  let module = Module("./main", "scratchpad://");
  let require = Require(loader, module);

  let tabs = require("sdk/tabs");

  tabs.open({
    url: "about:blank",
    onReady: function (tab) {
      is(tab.url, "about:blank", "correct uri for tab");
      is(tabs.activeTab, tab, "correctly active tab");
      tab.close(finish);
    }
  });
}
