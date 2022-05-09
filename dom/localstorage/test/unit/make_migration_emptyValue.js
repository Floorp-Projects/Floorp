/*
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
*/

async function testSteps() {
  const data = {
    key: "foo",
    value: "",
  };

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", false);

  info("Getting storage");

  const storage = getLocalStorage();

  info("Adding data");

  storage.setItem(data.key, data.value);
}
