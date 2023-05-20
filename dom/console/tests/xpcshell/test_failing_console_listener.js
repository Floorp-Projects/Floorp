/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  Assert.ok("console" in this);

  // Add a first listener that synchronously throws.
  const removeListener1 = addConsoleListener(() => {
    throw new Error("Fail");
  });

  // Add a second listener updating a flag we can observe from the test.
  let secondListenerCalled = false;
  const removeListener2 = addConsoleListener(
    () => (secondListenerCalled = true)
  );

  console.log(42);
  Assert.ok(secondListenerCalled, "Second listener was called");

  // Cleanup listeners.
  removeListener1();
  removeListener2();
});

function addConsoleListener(callback) {
  const principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );
  ConsoleAPIStorage.addLogEventListener(callback, principal);

  return () => {
    ConsoleAPIStorage.removeLogEventListener(callback, principal);
  };
}
