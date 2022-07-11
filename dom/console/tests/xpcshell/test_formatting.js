/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  Assert.ok("console" in this);

  const tests = [
    // Plain value.
    [[42], ["42"]],

    // Integers.
    [["%d", 42], ["42"]],
    [["%i", 42], ["42"]],
    [["c%iao", 42], ["c42ao"]],

    // Floats.
    [["%2.4f", 42], ["42.0000"]],
    [["%2.2f", 42], ["42.00"]],
    [["%1.2f", 42], ["42.00"]],
    [["a%3.2fb", 42], ["a42.00b"]],
    [["%f", NaN], ["NaN"]],

    // Strings
    [["%s", 42], ["42"]],

    // Empty values.
    [
      ["", 42],
      ["", "42"],
    ],
    [
      ["", 42],
      ["", "42"],
    ],
  ];

  let p = new Promise(resolve => {
    let t = 0;

    function consoleListener() {
      addConsoleStorageListener(this);
    }

    consoleListener.prototype = {
      observe(aSubject) {
        let test = tests[t++];

        let obj = aSubject.wrappedJSObject;
        Assert.equal(
          obj.arguments.length,
          test[1].length,
          "Same number of arguments"
        );
        for (let i = 0; i < test[1].length; ++i) {
          Assert.equal(
            "" + obj.arguments[i],
            test[1][i],
            "Message received: " + test[1][i]
          );
        }

        if (t === tests.length) {
          removeConsoleStorageListener(this);
          resolve();
        }
      },
    };

    new consoleListener();
  });

  tests.forEach(test => {
    console.log(...test[0]);
  });

  await p;
});
