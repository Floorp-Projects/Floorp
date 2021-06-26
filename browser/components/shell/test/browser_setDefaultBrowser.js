/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetters(this, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
});

const userChoiceStub = sinon
  .stub(ShellService, "setAsDefaultUserChoice")
  .resolves();
const setDefaultStub = sinon.stub();
const shellStub = sinon
  .stub(ShellService, "shellService")
  .value({ setDefaultBrowser: setDefaultStub });
registerCleanupFunction(() => {
  userChoiceStub.restore();
  shellStub.restore();

  ExperimentAPI._store._deleteForTests("shellService");
});

let defaultUserChoice;
add_task(async function need_user_choice() {
  ShellService.setDefaultBrowser();
  defaultUserChoice = userChoiceStub.called;

  Assert.ok(
    defaultUserChoice !== undefined,
    "Decided which default browser method to use"
  );
  Assert.equal(
    setDefaultStub.notCalled,
    defaultUserChoice,
    "Only one default behavior was used"
  );
});

add_task(async function remote_disable() {
  if (defaultUserChoice === false) {
    info("Default behavior already not user choice, so nothing to test");
    return;
  }

  userChoiceStub.resetHistory();
  setDefaultStub.resetHistory();
  await ExperimentFakes.remoteDefaultsHelper({
    feature: NimbusFeatures.shellService,
    configuration: { variables: { setDefaultBrowserUserChoice: false } },
  });

  ShellService.setDefaultBrowser();

  Assert.ok(
    userChoiceStub.notCalled,
    "Set default with user choice disabled via nimbus"
  );
  Assert.ok(setDefaultStub.called, "Used plain set default insteead");
});

add_task(async function restore_default() {
  if (defaultUserChoice === undefined) {
    info("No default user choice behavior set, so nothing to test");
    return;
  }

  userChoiceStub.resetHistory();
  setDefaultStub.resetHistory();
  await ExperimentFakes.remoteDefaultsHelper({
    feature: NimbusFeatures.shellService,
    configuration: {},
  });

  ShellService.setDefaultBrowser();

  Assert.equal(
    userChoiceStub.called,
    defaultUserChoice,
    "Set default with user choice restored to original"
  );
  Assert.equal(
    setDefaultStub.notCalled,
    defaultUserChoice,
    "Plain set default behavior restored to original"
  );
});
