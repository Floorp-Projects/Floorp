"use strict";

let { SyncedTabsDeckStore } = ChromeUtils.import(
  "resource:///modules/syncedtabs/SyncedTabsDeckStore.js"
);

add_task(async function testSelectUnkownPanel() {
  let deckStore = new SyncedTabsDeckStore();
  let spy = sinon.spy();

  deckStore.on("change", spy);
  deckStore.selectPanel("foo");

  Assert.ok(!spy.called);
});

add_task(async function testSetPanels() {
  let deckStore = new SyncedTabsDeckStore();
  let spy = sinon.spy();

  deckStore.on("change", spy);
  deckStore.setPanels(["panel1", "panel2"]);

  Assert.ok(
    spy.calledWith({
      panels: [
        { id: "panel1", selected: false },
        { id: "panel2", selected: false },
      ],
      isUpdatable: false,
    })
  );
});

add_task(async function testSelectPanel() {
  let deckStore = new SyncedTabsDeckStore();
  let spy = sinon.spy();

  deckStore.setPanels(["panel1", "panel2"]);

  deckStore.on("change", spy);
  deckStore.selectPanel("panel2");

  Assert.ok(
    spy.calledWith({
      panels: [
        { id: "panel1", selected: false },
        { id: "panel2", selected: true },
      ],
      isUpdatable: true,
    })
  );

  deckStore.selectPanel("panel2");
  Assert.ok(spy.calledOnce, "doesn't trigger unless panel changes");
});

add_task(async function testSetPanelsSameArray() {
  let deckStore = new SyncedTabsDeckStore();
  let spy = sinon.spy();
  deckStore.on("change", spy);

  let panels = ["panel1", "panel2"];

  deckStore.setPanels(panels);
  deckStore.setPanels(panels);

  Assert.ok(spy.calledOnce, "doesn't trigger unless set of panels changes");
});
