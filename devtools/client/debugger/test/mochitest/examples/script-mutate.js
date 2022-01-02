/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
function mutate() {
  const phonebook = {
    S: {
      sarah: {
        lastName: "Doe"
      },
      serena: {
        lastName: "Williams"
      }
    }
  };

  debugger;
  phonebook.S.sarah.lastName = "Pierce";
  debugger;
  phonebook.S.sarah.timezone = "PST";
  debugger;
}
