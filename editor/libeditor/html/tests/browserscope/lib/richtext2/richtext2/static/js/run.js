/**
 * @fileoverview 
 * Main functions used in running the RTE test suite.
 *
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the 'License')
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @version 0.1
 * @author rolandsteiner@google.com
 */

/**
 * Info function: returns true if the suite (mainly) tests the result HTML/Text.
 *
 * @param suite {String} the test suite
 * @return {boolean} Whether the suite main focus is the output HTML/Text
 */
function suiteChecksHTMLOrText(suite) {
  return suite.id[0] != 'S';
}

/**
 * Info function: returns true if the suite checks the result selection.
 *
 * @param suite {String} the test suite
 * @return {boolean} Whether the suite checks the selection
 */
function suiteChecksSelection(suite) {
  return suite.id[0] != 'Q';
}

/**
 * Helper function returning the effective value of a test parameter.
 *
 * @param suite {Object} the test suite
 * @param group {Object} group of tests within the suite the test belongs to
 * @param test {Object} the test
 * @param param {String} the test parameter to be checked
 * @return {Any} the effective value of the parameter (can be undefined)
 */
function getTestParameter(suite, group, test, param) {
  var val = test[param];
  if (val === undefined) {
    val = group[param];
  }
  if (val === undefined) {
    val = suite[param];
  }
  return val;
}

/**
 * Helper function returning the effective value of a container/test parameter.
 *
 * @param suite {Object} the test suite
 * @param group {Object} group of tests within the suite the test belongs to
 * @param test {Object} the test
 * @param container {Object} the container descriptor object
 * @param param {String} the test parameter to be checked
 * @return {Any} the effective value of the parameter (can be undefined)
 */
function getContainerParameter(suite, group, test, container, param) {
  var val = undefined;
  if (test[container.id]) {
    val = test[container.id][param];
  }
  if (val === undefined) {
    val = test[param];
  }
  if (val === undefined) {
    val = group[param];
  }
  if (val === undefined) {
    val = suite[param];
  }
  return val;
}

/**
 * Initializes the global variables before any tests are run.
 */
function initVariables() {
  results = {
      count: 0,
      valscore: 0,
      selscore: 0
  };
}

/**
 * Runs a single test - outputs and sets the result variables.
 *
 * @param suite {Object} suite that test originates in as object reference
 * @param group {Object} group of tests within the suite the test belongs to
 * @param test {Object} test to be run as object reference
 * @param container {Object} container descriptor as object reference
 * @see variables.js for RESULT... values
 */
function runSingleTest(suite, group, test, container) {
  var result = {
    valscore: 0,
    selscore: 0,
    valresult: VALRESULT_NOT_RUN,
    selresult: SELRESULT_NOT_RUN,
    output: ''
  };

  // 1.) Populate the editor element with the initial test setup HTML.
  try {
    initContainer(suite, group, test, container);
  } catch(ex) {
    result.valresult = VALRESULT_SETUP_EXCEPTION;
    result.selresult = SELRESULT_NA;
    result.output = SETUP_EXCEPTION + ex.toString();
    return result;
  }

  // 2.) Run the test command, general function or query function.
  var isHTMLTest = false;

  try {
    var cmd = undefined;

    if (cmd = getTestParameter(suite, group, test, PARAM_EXECCOMMAND)) {
      isHTMLTest = true;
      // Note: "getTestParameter(suite, group, test, PARAM_VALUE) || null"
      // doesn't work, since value might be the empty string, e.g., for 'insertText'!
      var value = getTestParameter(suite, group, test, PARAM_VALUE);
      if (value === undefined) {
        value = null;
      }
      container.doc.execCommand(cmd, false, value);
    } else if (cmd = getTestParameter(suite, group, test, PARAM_FUNCTION)) {
      isHTMLTest = true;
      eval(cmd);
    } else if (cmd = getTestParameter(suite, group, test, PARAM_QUERYCOMMANDSUPPORTED)) {
      result.output = container.doc.queryCommandSupported(cmd);
    } else if (cmd = getTestParameter(suite, group, test, PARAM_QUERYCOMMANDENABLED)) {
      result.output = container.doc.queryCommandEnabled(cmd);
    } else if (cmd = getTestParameter(suite, group, test, PARAM_QUERYCOMMANDINDETERM)) {
      result.output = container.doc.queryCommandIndeterm(cmd);
    } else if (cmd = getTestParameter(suite, group, test, PARAM_QUERYCOMMANDSTATE)) {
      result.output = container.doc.queryCommandState(cmd);
    } else if (cmd = getTestParameter(suite, group, test, PARAM_QUERYCOMMANDVALUE)) {
      result.output = container.doc.queryCommandValue(cmd);
      if (result.output === false) {
        // A return value of boolean 'false' for queryCommandValue means 'not supported'.
        result.valresult = VALRESULT_UNSUPPORTED;
        result.selresult = SELRESULT_NA;
        result.output = UNSUPPORTED;
        return result;
      }
    } else {
      result.valresult = VALRESULT_SETUP_EXCEPTION;
      result.selresult = SELRESULT_NA;
      result.output = SETUP_EXCEPTION + SETUP_NOCOMMAND;
      return result;
    }
  } catch (ex) {
    result.valresult = VALRESULT_EXECUTION_EXCEPTION;
    result.selresult = SELRESULT_NA;
    result.output = EXECUTION_EXCEPTION + ex.toString();
    return result;
  }
  
  // 4.) Verify test result
  try {
    if (isHTMLTest) {
      // First, retrieve HTML from container
      prepareHTMLTestResult(container, result);

      // Compare result to expectations
      compareHTMLTestResult(suite, group, test, container, result);

      result.valscore = (result.valresult === VALRESULT_EQUAL) ? 1 : 0;
      result.selscore = (result.selresult === SELRESULT_EQUAL) ? 1 : 0;
    } else {
      compareTextTestResult(suite, group, test, result);

      result.selresult = SELRESULT_NA;
      result.valscore = (result.valresult === VALRESULT_EQUAL) ? 1 : 0;
    }
  } catch (ex) {
    result.valresult = VALRESULT_VERIFICATION_EXCEPTION;
    result.selresult = SELRESULT_NA;
    result.output = VERIFICATION_EXCEPTION + ex.toString();
    return result;
  }
  
  return result;
}

/**
 * Initializes the results dictionary for a given test suite.
 * (for all classes -> tests -> containers)
 *
 * @param {Object} suite as object reference
 */
function initTestSuiteResults(suite) {
  var suiteID = suite.id;

  // Initialize results entries for this suite
  results[suiteID] = {
      count: 0,
      valscore: 0,
      selscore: 0,
      time: 0
  };
  var totalTestCount = 0;

  for (var clsIdx = 0; clsIdx < testClassCount; ++clsIdx) {
    var clsID = testClassIDs[clsIdx];
    var cls = suite[clsID];
    if (!cls)
      continue;

    results[suiteID][clsID] = {
        count: 0,
        valscore: 0,
        selscore: 0
    };
    var clsTestCount = 0;

    var groupCount = cls.length;
    for (var groupIdx = 0; groupIdx < groupCount; ++groupIdx) {
      var group = cls[groupIdx];
      var testCount = group.tests.length;

      clsTestCount += testCount;
      totalTestCount += testCount;

      for (var testIdx = 0; testIdx < testCount; ++testIdx) {
        var test = group.tests[testIdx];
        
        results[suiteID][clsID ][test.id] = {
            valscore: 0,
            selscore: 0,
            valresult: VALRESULT_NOT_RUN,
            selresult: SELRESULT_NOT_RUN
        };
        for (var cntIdx = 0; cntIdx < containers.length; ++cntIdx) {
          var cntID = containers[cntIdx].id;

          results[suiteID][clsID][test.id][cntID] = {
            valscore: 0,
            selscore: 0,
            valresult: VALRESULT_NOT_RUN,
            selresult: SELRESULT_NOT_RUN,
            output: ''
          }
        }
      }
    }
    results[suiteID][clsID].count = clsTestCount;
  }
  results[suiteID].count = totalTestCount;
}

/**
 * Runs a single test suite (such as DELETE tests or INSERT tests).
 *
 * @param suite {Object} suite as object reference
 */
function runTestSuite(suite) {
  var suiteID = suite.id;
  var suiteStartTime = new Date().getTime();

  initTestSuiteResults(suite);

  for (var clsIdx = 0; clsIdx < testClassCount; ++clsIdx) {
    var clsID = testClassIDs[clsIdx];
    var cls = suite[clsID];
    if (!cls)
      continue;

    var groupCount = cls.length;

    for (var groupIdx = 0; groupIdx < groupCount; ++groupIdx) {
      var group = cls[groupIdx];
      var testCount = group.tests.length;

      for (var testIdx = 0; testIdx < testCount; ++testIdx) {
        var test = group.tests[testIdx];

        var valscore = 1;
        var selscore = 1;
        var valresult = VALRESULT_EQUAL;
        var selresult = SELRESULT_EQUAL;

        for (var cntIdx = 0; cntIdx < containers.length; ++cntIdx) {
          var container = containers[cntIdx];
          var cntID = container.id;

          var result = runSingleTest(suite, group, test, container);

          results[suiteID][clsID][test.id][cntID] = result;

          valscore = Math.min(valscore, result.valscore);
          selscore = Math.min(selscore, result.selscore);
          valresult = Math.min(valresult, result.valresult);
          selresult = Math.min(selresult, result.selresult);

          resetContainer(container);
        }          

        results[suiteID][clsID][test.id].valscore = valscore;
        results[suiteID][clsID][test.id].selscore = selscore;
        results[suiteID][clsID][test.id].valresult = valresult;
        results[suiteID][clsID][test.id].selresult = selresult;

        results[suiteID][clsID].valscore += valscore;
        results[suiteID][clsID].selscore += selscore;
        results[suiteID].valscore += valscore;
        results[suiteID].selscore += selscore;
        results.valscore += valscore;
        results.selscore += selscore;
      }
    }
  }

  results[suiteID].time = new Date().getTime() - suiteStartTime;
}

/**
 * Runs a single test suite (such as DELETE tests or INSERT tests)
 * and updates the output HTML.
 *
 * @param {Object} suite as object reference
 */
function runAndOutputTestSuite(suite) {
  runTestSuite(suite);
  outputTestSuiteResults(suite);
}

/**
 * Fills the beacon with the test results.
 */
function fillResults() {
  // Result totals of the individual categories
  categoryTotals = [
    'selection='        + results['S'].selscore,
    'apply='            + results['A'].valscore,
    'applyCSS='         + results['AC'].valscore,
    'change='           + results['C'].valscore,
    'changeCSS='        + results['CC'].valscore,
    'unapply='          + results['U'].valscore,
    'unapplyCSS='       + results['UC'].valscore,
    'delete='           + results['D'].valscore,
    'forwarddelete='    + results['FD'].valscore,
    'insert='           + results['I'].valscore,
    'selectionResult='  + (results['A'].selscore +
                           results['AC'].selscore +
                           results['C'].selscore +
                           results['CC'].selscore +
                           results['U'].selscore +
                           results['UC'].selscore +
                           results['D'].selscore +
                           results['FD'].selscore +
                           results['I'].selscore),
    'querySupported='   + results['Q'].valscore,
    'queryEnabled='     + results['QE'].valscore,
    'queryIndeterm='    + results['QI'].valscore,
    'queryState='       + results['QS'].valscore,
    'queryStateCSS='    + results['QSC'].valscore,
    'queryValue='       + results['QV'].valscore,
    'queryValueCSS='    + results['QVC'].valscore
  ];
  
  // Beacon copies category results
  beacon = categoryTotals.slice(0);
}

