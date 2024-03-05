/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test object with private properties (preview + enumPrivateProperties)

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

function evalCode(debuggee) {
  debuggee.eval(
    // These arguments are tested.
    // eslint-disable-next-line no-unused-vars
    function stopMe(obj) {
      debugger;
    }.toString()
  );
  debuggee.eval(`
    class MyClass {
      constructor(name, password) {
        this.name = name;
        this.#password = password;
      }

      #password;
      #salt = "sEcr3t";
      #getSaltedPassword() {
        return this.#password + this.#salt;
      }
    }

    stopMe(new MyClass("Susie", "p4$$w0rD"));
  `);
}

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const [grip] = packet.frame.arguments;

    let { privateProperties } = grip.preview;
    strictEqual(
      privateProperties.length,
      2,
      "There is 2 private properties in the grip preview"
    );
    let [password, salt] = privateProperties;

    strictEqual(
      password.name,
      "#password",
      "Got expected name for #password private property in preview"
    );
    deepEqual(
      password.descriptor,
      {
        configurable: true,
        enumerable: false,
        writable: true,
        value: "p4$$w0rD",
      },
      "Got expected property descriptor for #password in preview"
    );

    strictEqual(
      salt.name,
      "#salt",
      "Got expected name for #salt private property in preview"
    );
    deepEqual(
      salt.descriptor,
      {
        configurable: true,
        enumerable: false,
        writable: true,
        value: "sEcr3t",
      },
      "Got expected property descriptor for #salt in preview"
    );

    const objClient = threadFront.pauseGrip(grip);
    const iterator = await objClient.enumPrivateProperties();
    ({ privateProperties } = await iterator.slice(0, iterator.count));

    strictEqual(
      privateProperties.length,
      2,
      "enumPrivateProperties returned 2 private properties."
    );
    [password, salt] = privateProperties;

    strictEqual(
      password.name,
      "#password",
      "Got expected name for #password private property via enumPrivateProperties"
    );
    deepEqual(
      password.descriptor,
      {
        configurable: true,
        enumerable: false,
        writable: true,
        value: "p4$$w0rD",
      },
      "Got expected property descriptor for #password via enumPrivateProperties"
    );

    strictEqual(
      salt.name,
      "#salt",
      "Got expected name for #salt private property via enumPrivateProperties"
    );
    deepEqual(
      salt.descriptor,
      {
        configurable: true,
        enumerable: false,
        writable: true,
        value: "sEcr3t",
      },
      "Got expected property descriptor for #salt via enumPrivateProperties"
    );

    await threadFront.resume();
  })
);
