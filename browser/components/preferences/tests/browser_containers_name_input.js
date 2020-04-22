const CONTAINERS_URL =
  "chrome://browser/content/preferences/dialogs/containers.xhtml";

add_task(async function setup() {
  await openPreferencesViaOpenPreferencesAPI("containers", { leaveOpen: true });
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function() {
  async function openDialog() {
    let doc = gBrowser.selectedBrowser.contentDocument;

    let dialogPromise = promiseLoadSubDialog(CONTAINERS_URL);

    let addButton = doc.getElementById("containersAdd");
    addButton.doCommand();

    let dialog = await dialogPromise;

    return dialog.document;
  }

  let doc = await openDialog();

  let name = doc.getElementById("name");
  let btnApplyChanges = doc.getElementById("btnApplyChanges");

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
});
