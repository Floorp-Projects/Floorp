var timeout=2000;

function performActionIfNeed() {
  action = Packages.org.mozilla.pluglet.test.basic.scenario.SequencialScenarioTest.getActionClear();
  if (action) {
    if (action == "BACK") {
      window.back();

    } else if (action == "FORWARD") { 
      window.forward();

    } else if (action == "LOAD_DEFAULT_BACK_URL") {
      window.location = "back.html";

    } else if (action == "WINDOW_RELOAD") {
      window.location.reload();

    } else if (action == "WINDOW_CLOSE") {
      window.close();
    }

  }  

  setTimeout("performActionIfNeed()",timeout);
}

