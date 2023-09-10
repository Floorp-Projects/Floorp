/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
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
  let doCleanup = await ExperimentFakes.enrollWithRollout({
    featureId: NimbusFeatures.shellService.featureId,
    value: {
      setDefaultBrowserUserChoice: false,
      enabled: true,
    },
  });

  ShellService.setDefaultBrowser();

  Assert.ok(
    userChoiceStub.notCalled,
    "Set default with user choice disabled via nimbus"
  );
  Assert.ok(setDefaultStub.called, "Used plain set default insteead");

  await doCleanup();
});

add_task(async function restore_default() {
  if (defaultUserChoice === undefined) {
    info("No default user choice behavior set, so nothing to test");
    return;
  }

  userChoiceStub.resetHistory();
  setDefaultStub.resetHistory();
  ExperimentAPI._store._deleteForTests("shellService");

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
