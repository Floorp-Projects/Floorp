const CONTAINERS_URL =
  "chrome://browser/content/preferences/dialogs/containers.xhtml";

add_setup(async function () {
  await openPreferencesViaOpenPreferencesAPI("containers", { leaveOpen: true });
  registerCleanupFunction(async function () {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function () {
  async function openDialog() {
    let doc = gBrowser.selectedBrowser.contentDocument;

    let dialogPromise = promiseLoadSubDialog(CONTAINERS_URL);

    let addButton = doc.getElementById("containersAdd");
    addButton.doCommand();

    let dialog = await dialogPromise;

    return dialog.document;
  }

  let { contentDocument } = gBrowser.selectedBrowser;
  let containerNodes = Array.from(
    contentDocument.querySelectorAll("[data-category=paneContainers]")
  );
  ok(
    containerNodes.find(node => node.getBoundingClientRect().width > 0),
    "Should actually be showing the container nodes."
  );

  let doc = await openDialog();

  let name = doc.getElementById("name");
  let btnApplyChanges = doc.querySelector("dialog").getButton("accept");

  Assert.equal(name.value, "", "The name textbox should initlally be empty");
  Assert.ok(
    btnApplyChanges.disabled,
    "The done button should initially be disabled"
  );

  function setName(value) {
    name.value = value;

    let event = new doc.defaultView.InputEvent("input", { data: value });
    SpecialPowers.dispatchEvent(doc.defaultView, name, event);
  }

  setName("test");

  Assert.ok(
    !btnApplyChanges.disabled,
    "The done button should be enabled when the value is not empty"
  );

  setName("");

  Assert.ok(
    btnApplyChanges.disabled,
    "The done button should be disabled when the value is empty"
  );

  setName("\u0009\u000B\u000C\u0020\u00A0\uFEFF\u000A\u000D\u2028\u2029");

  Assert.ok(
    btnApplyChanges.disabled,
    "The done button should be disabled when the value contains only whitespaces"
  );
});
