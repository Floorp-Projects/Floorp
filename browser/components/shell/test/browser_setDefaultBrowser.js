/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const setDefaultBrowserUserChoiceStub = async () => {
  throw Components.Exception("", Cr.NS_ERROR_WDBA_NO_PROGID);
};

const defaultAgentStub = sinon
  .stub(ShellService, "defaultAgent")
  .value({ setDefaultBrowserUserChoiceAsync: setDefaultBrowserUserChoiceStub });

const _userChoiceImpossibleTelemetryResultStub = sinon
  .stub(ShellService, "_userChoiceImpossibleTelemetryResult")
  .callsFake(() => null);

const userChoiceStub = sinon
  .stub(ShellService, "setAsDefaultUserChoice")
  .resolves();
const setDefaultStub = sinon.stub();
const shellStub = sinon
  .stub(ShellService, "shellService")
  .value({ setDefaultBrowser: setDefaultStub });

registerCleanupFunction(() => {
  defaultAgentStub.restore();
  _userChoiceImpossibleTelemetryResultStub.restore();
  userChoiceStub.restore();
  shellStub.restore();

  ExperimentAPI._store._deleteForTests("shellService");
});

let defaultUserChoice;
add_task(async function need_user_choice() {
  await ShellService.setDefaultBrowser();
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

  await ShellService.setDefaultBrowser();

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

  await ShellService.setDefaultBrowser();

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

add_task(async function ensure_fallback() {
  if (AppConstants.platform != "win") {
    info("Nothing to test on non-Windows");
    return;
  }

  let userChoicePromise = Promise.resolve();
  userChoiceStub.callsFake(function (...args) {
    return (userChoicePromise = userChoiceStub.wrappedMethod.apply(this, args));
  });
  userChoiceStub.resetHistory();
  setDefaultStub.resetHistory();
  let doCleanup = await ExperimentFakes.enrollWithRollout({
    featureId: NimbusFeatures.shellService.featureId,
    value: {
      setDefaultBrowserUserChoice: true,
      setDefaultPDFHandler: false,
      enabled: true,
    },
  });

  await ShellService.setDefaultBrowser();

  Assert.ok(userChoiceStub.called, "Set default with user choice called");

  let message = "";
  await userChoicePromise.catch(err => (message = err.message || ""));

  Assert.ok(
    message.includes("ErrExeProgID"),
    "Set default with user choice threw an expected error"
  );
  Assert.ok(setDefaultStub.called, "Fallbacked to plain set default");

  await doCleanup();
});
