/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const MULTIPLE_CHOICE = 0;
const CHECK_BOXES_WITH_FREE_ENTRY = 1;
const SCALE = 2;
const FREE_ENTRY = 3;
const CHECK_BOXES = 4;
const MULTIPLE_CHOICE_WITH_FREE_ENTRY = 5;

var stringBundle;

function onBuiltinSurveyLoad() {
  Components.utils.import("resource://testpilot/modules/setup.js");
  Components.utils.import("resource://testpilot/modules/tasks.js");
  setStrings();
  let eid = getUrlParam("eid");
  let task = TestPilotSetup.getTaskById(eid);
  let contentDiv = document.getElementById("survey-contents");
  let explanation = document.getElementById("survey-explanation");
  if (!task) {
    // Tasks haven't all loaded yet.  Try again in a few seconds.
    contentDiv.innerHTML =
      stringBundle.GetStringFromName("testpilot.surveyPage.loading");
    window.setTimeout(function() { onBuiltinSurveyLoad(); }, 2000);
    return;
  }

  let title = document.getElementById("survey-title");
  title.innerHTML = task.title;

  let submitButton = document.getElementById("survey-submit");
  if (task.relatedStudyId) {
     submitButton.innerHTML =
      stringBundle.GetStringFromName("testpilot.surveyPage.submitAnswers");
  } else {
    submitButton.innerHTML =
      stringBundle.GetStringFromName("testpilot.surveyPage.saveAnswers");
  }

  if (task.status == TaskConstants.STATUS_SUBMITTED) {
    contentDiv.innerHTML =
      "<p>" +
      stringBundle.GetStringFromName(
        "testpilot.surveyPage.thankYouForFinishingSurvey") + "</p><p>" +
      stringBundle.GetStringFromName(
        "testpilot.surveyPage.reviewOrChangeYourAnswers") + "</p>";
    explanation.innerHTML = "";
    submitButton.setAttribute("style", "display:none");
    let changeButton = document.getElementById("change-answers");
    changeButton.setAttribute("style", "");
  } else {
    contentDiv.innerHTML = "";
    if (task.surveyExplanation) {
      explanation.innerHTML = task.surveyExplanation;
    } else {
      explanation.innerHTML = "";
    }
    drawSurveyForm(task, contentDiv);
    // Allow surveys to define arbitrary page load handlers - call them
    // after creating the rest of the page:
    task.onPageLoad(task, document);
  }
}

function drawSurveyForm(task, contentDiv) {
  let surveyQuestions = task.surveyQuestions;

  /* Fill form fields with old survey answers if available --
   * but not if the survey version has changed since you stored them!!
   * (bug 576482) */
  let oldAnswers = null;
  if (task.oldAnswers && (task.version == task.oldAnswers["version_number"])) {
    oldAnswers = task.oldAnswers["answers"];
  }

  let submitButton = document.getElementById("survey-submit");
  submitButton.setAttribute("style", "");
  let changeButton = document.getElementById("change-answers");
  changeButton.setAttribute("style", "display:none");
  // Loop through questions and render html form input elements for each
  // one.
  for (let i = 0; i < surveyQuestions.length; i++) {
    let question = surveyQuestions[i].question;
    let explanation = surveyQuestions[i].explanation;
    let elem, j;

    elem = document.createElement("h3");
    elem.innerHTML = (i+1) + ". " + question;
    contentDiv.appendChild(elem);
    if (explanation) {
      elem = document.createElement("p");
      elem.setAttribute("class", "survey-question-explanation");
      elem.innerHTML = explanation;
      contentDiv.appendChild(elem);
    }
    // If you've done this survey before, preset all inputs using old answers
    let choices = surveyQuestions[i].choices;
    switch (surveyQuestions[i].type) {
    case MULTIPLE_CHOICE:
      for (j = 0; j < choices.length; j++) {
        let newRadio = document.createElement("input");
        newRadio.setAttribute("type", "radio");
        newRadio.setAttribute("name", "answer_to_" + i);
        newRadio.setAttribute("value", j);
        if (oldAnswers && oldAnswers[i] == String(j)) {
          newRadio.setAttribute("checked", "true");
        }
        let label = document.createElement("span");
        label.innerHTML = choices[j];
        contentDiv.appendChild(newRadio);
        contentDiv.appendChild(label);
        contentDiv.appendChild(document.createElement("br"));
      }
      break;
    case CHECK_BOXES:
    case CHECK_BOXES_WITH_FREE_ENTRY:
      let checkboxName = "answer_to_" + i;
      // Check boxes:
      for (j = 0; j < choices.length; j++) {
        let newCheck = document.createElement("input");
        newCheck.setAttribute("type", "checkbox");
        newCheck.setAttribute("name", checkboxName);
        newCheck.setAttribute("value", j);
        if (oldAnswers && oldAnswers[i]) {
          for each (let an in oldAnswers[i]) {
            if (an == String(j)) {
              newCheck.setAttribute("checked", "true");
              break;
            }
          }
        }
        let label = document.createElement("span");
        label.innerHTML = choices[j];
        contentDiv.appendChild(newCheck);
        contentDiv.appendChild(label);
        contentDiv.appendChild(document.createElement("br"));
      }
      // Text area:
      if (surveyQuestions[i].type == CHECK_BOXES_WITH_FREE_ENTRY &&
          surveyQuestions[i].free_entry) {
        let freeformId = "freeform_" + i;
        let newCheck = document.createElement("input");
        newCheck.setAttribute("type", "checkbox");
        newCheck.setAttribute("name", checkboxName);
        newCheck.setAttribute("value", freeformId);
        newCheck.addEventListener(
          "click", function(event) {
            if (!event.target.checked) {
              document.getElementById(freeformId).value = "";
            }
          }, false);
        let label = document.createElement("span");
        label.innerHTML = surveyQuestions[i].free_entry + "&nbsp:&nbsp";
        let inputBox = document.createElement("textarea");
        inputBox.setAttribute("id", freeformId);
        inputBox.addEventListener(
          "keypress", function() {
            let elements = document.getElementsByName(checkboxName);
            for (j = (elements.length - 1); j >= 0; j--) {
              if (elements[j].value == freeformId) {
                elements[j].checked = true;
                break;
              }
            }
          }, false);
        if (oldAnswers && oldAnswers[i]) {
          for each (let an in oldAnswers[i]) {
            if (isNaN(parseInt(an))) {
              newCheck.setAttribute("checked", "true");
              inputBox.value = an;
              break;
            }
          }
        }
        contentDiv.appendChild(newCheck);
        contentDiv.appendChild(label);
        contentDiv.appendChild(inputBox);
      }
      break;
    case SCALE:
      let label = document.createElement("span");
      label.innerHTML = surveyQuestions[i].min_label;
      contentDiv.appendChild(label);
      for (j = surveyQuestions[i].scale_minimum;
           j <= surveyQuestions[i].scale_maximum;
           j++) {
        let newRadio = document.createElement("input");
        newRadio.setAttribute("type", "radio");
        newRadio.setAttribute("name", "answer_to_" + i);
        newRadio.setAttribute("value", j);
        if (oldAnswers && oldAnswers[i] == String(j)) {
          newRadio.setAttribute("checked", "true");
        }
        contentDiv.appendChild(newRadio);
      }
      label = document.createElement("span");
      label.innerHTML = surveyQuestions[i].max_label;
      contentDiv.appendChild(label);
      break;
    case FREE_ENTRY:
      let inputBox = document.createElement("textarea");
      inputBox.setAttribute("id", "freeform_" + i);

      if (oldAnswers && oldAnswers[i] && (oldAnswers[i].length > 0)) {
        inputBox.value = oldAnswers[i];
      }
      contentDiv.appendChild(inputBox);
      break;
    case MULTIPLE_CHOICE_WITH_FREE_ENTRY:
      let checked = false;
      let freeformId = "freeform_" + i;
      let radioName = "answer_to_" + i;

      for (j = 0; j < choices.length; j++) {
        let newRadio = document.createElement("input");
        newRadio.setAttribute("type", "radio");
        newRadio.setAttribute("name", radioName);
        newRadio.setAttribute("value", j);
        newRadio.addEventListener(
          "click", function() {
            let inputBox = document.getElementById(freeformId);
            if (inputBox) {
              inputBox.value = "";
            }
          }, false);
        let label = document.createElement("span");
        label.innerHTML = choices[j];
        if (oldAnswers && oldAnswers[i] == String(j)) {
          newRadio.setAttribute("checked", "true");
          checked = true;
        }
        contentDiv.appendChild(newRadio);
        contentDiv.appendChild(label);
        contentDiv.appendChild(document.createElement("br"));
      }

      // Text area:
      if (surveyQuestions[i].free_entry) {
        let newRadio = document.createElement("input");
        newRadio.setAttribute("type", "radio");
        newRadio.setAttribute("name", radioName);
        newRadio.setAttribute("value", freeformId);
        let label = document.createElement("span");
        label.innerHTML = surveyQuestions[i].free_entry + "&nbsp:&nbsp";
        let inputBox = document.createElement("textarea");
        inputBox.setAttribute("id", freeformId);
        inputBox.addEventListener(
          "keypress", function() {
            let elements = document.getElementsByName(radioName);
            for (let j = 0; j < elements.length; j++) {
              if (elements[j].value == freeformId) {
                elements[j].checked = true;
              } else {
                elements[j].checked = false;
              }
            }
          }, false);
       if (oldAnswers && oldAnswers[i] && (oldAnswers[i].length > 0) &&
            !checked) {
          newRadio.setAttribute("checked", "true");
          inputBox.value = oldAnswers[i];
        }
        contentDiv.appendChild(newRadio);
        contentDiv.appendChild(label);
        contentDiv.appendChild(inputBox);
      }
      break;
    }
  }
}

function onBuiltinSurveySubmit() {
  Components.utils.import("resource://testpilot/modules/setup.js");
  Components.utils.import("resource://testpilot/modules/log4moz.js");
  let logger = Log4Moz.repository.getLogger("TestPilot.Survey");
  let eid = getUrlParam("eid");
  let task = TestPilotSetup.getTaskById(eid);
  logger.info("Storing survey answers for survey id " + eid);

  // Read all values from form...
  let answers = [];
  let surveyQuestions = task.surveyQuestions;
  let i;
  for (i = 0; i < surveyQuestions.length; i++) {
    let elems = document.getElementsByName("answer_to_" + i);
    let anAnswer = [];
    for each (let elem in elems) {
      if (elem.checked && elem.value != ("freeform_" + i)) {
        anAnswer.push(elem.value);
      }
    }
    let freeEntry = document.getElementById("freeform_" + i);
    if (freeEntry && freeEntry.value) {
      anAnswer.push(freeEntry.value);
    }
    answers.push(anAnswer);
  }
  let surveyResults = { answers: answers };
  logger.info("Storing survey answers " + JSON.stringify(surveyResults));
  task.store(surveyResults, function(submitted) {
    // Reload page to show submitted status:
    if (submitted) {
      onBuiltinSurveyLoad();
    }
  });
}

function onBuiltinSurveyChangeAnswers() {
  let eid = getUrlParam("eid");
  let task = TestPilotSetup.getTaskById(eid);
  let contentDiv = document.getElementById("survey-contents");

  drawSurveyForm(task, contentDiv);
}

function setStrings() {
  stringBundle =
    Components.classes["@mozilla.org/intl/stringbundle;1"].
      getService(Components.interfaces.nsIStringBundleService).
        createBundle("chrome://testpilot/locale/main.properties");
  let map = [
    { id: "page-title", stringKey: "testpilot.fullBrandName" },
    { id: "comments-and-discussions-link",
      stringKey: "testpilot.page.commentsAndDiscussions" },
    { id: "propose-test-link",
      stringKey: "testpilot.page.proposeATest" },
    { id: "testpilot-twitter-link",
      stringKey: "testpilot.page.testpilotOnTwitter" },
    { id: "change-answers",
      stringKey: "testpilot.surveyPage.changeAnswers" }
  ];
  let mapLength = map.length;
  for (let i = 0; i < mapLength; i++) {
    let entry = map[i];
    document.getElementById(entry.id).innerHTML =
      stringBundle.GetStringFromName(entry.stringKey);
  }
}
