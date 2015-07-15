/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * Returns true if res is a local rtp
 *
 * @param {Object} statObject
 *        One of the objects comprising the report received from getStats()
 * @returns {boolean}
 *        True if object is a local rtp
 */
function isLocalRtp(statObject) {
  return (typeof statObject === 'object' &&
          statObject.isRemote === false);
}


/**
 * Dumps the local, dynamic parts of the stats object as a formatted block
 * Used for capturing and monitoring test status during execution
 *
 * @param {Object} stats
 *        Stats object to use for output
 * @param {string} label
 *        Used in the header of the output
 */
function outputPcStats(stats, label) {
  var outputStr = '\n\n';
  function appendOutput(line) {
    outputStr += line.toString() + '\n';
  }

  var firstRtp = true;
  for (var prop in stats) {
    if (isLocalRtp(stats[prop])) {
      var rtp = stats[prop];
      if (firstRtp) {
        appendOutput(label.toUpperCase() + ' STATS ' +
                     '(' + new Date(rtp.timestamp).toISOString() + '):');
        firstRtp = false;
      }
      appendOutput('  ' + rtp.id + ':');
      if (rtp.type === 'inboundrtp') {
        appendOutput('    bytesReceived: ' + rtp.bytesReceived);
        appendOutput('    jitter: ' + rtp.jitter);
        appendOutput('    packetsLost: ' + rtp.packetsLost);
        appendOutput('    packetsReceived: ' + rtp.packetsReceived);
      } else {
        appendOutput('    bytesSent: ' + rtp.bytesSent);
        appendOutput('    packetsSent: ' + rtp.packetsSent);
      }
    }
  }
  outputStr += '\n\n';
  dump(outputStr);
}


var _lastStats = {};

const MAX_ERROR_CYCLES = 5;
var _errorCount = {};

/**
 * Verifies the peer connection stats interval over interval
 *
 * @param {Object} stats
 *        Stats object to use for verification
 * @param {string} label
 *        Identifies the peer connection. Differentiates stats for
 *        interval-over-interval verification in cases where more than one set
 *        is being verified
 */
function verifyPcStats(stats, label) {
  const INCREASING_INBOUND_STAT_NAMES = [
    'bytesReceived',
    'packetsReceived'
  ];

  const INCREASING_OUTBOUND_STAT_NAMES = [
    'bytesSent',
    'packetsSent'
  ];

  if (_lastStats[label] !== undefined) {
    var errorsInCycle = false;

    function verifyIncrease(rtpName, statNames) {
      var timestamp = new Date(stats[rtpName].timestamp).toISOString();

      statNames.forEach(function (statName) {
        var passed = stats[rtpName][statName] >
            _lastStats[label][rtpName][statName];
        if (!passed) {
          errorsInCycle = true;
        }
        ok(passed,
           timestamp + '.' + label + '.' + rtpName + '.' + statName,
           label + '.' + rtpName + '.' + statName + ' increased (value=' +
           stats[rtpName][statName] + ')');
      });
    }

    for (var prop in stats) {
      if (isLocalRtp(stats[prop])) {
        if (stats[prop].type === 'inboundrtp') {
          verifyIncrease(prop, INCREASING_INBOUND_STAT_NAMES);
        } else {
          verifyIncrease(prop, INCREASING_OUTBOUND_STAT_NAMES);
        }
      }
    }

    if (errorsInCycle) {
      _errorCount[label] += 1;
      info(label +": increased error counter to " + _errorCount[label]);
    } else {
      // looks like we recovered from a temp glitch
      if (_errorCount[label] > 0) {
        info(label + ": reseting error counter to zero");
      }
      _errorCount[label] = 0;
    }
  } else {
    _errorCount[label] = 0;
  }

  _lastStats[label] = stats;
}


/**
 * Retrieves and performs a series of operations on PeerConnection stats
 *
 * @param {PeerConnectionWrapper} pc
 *        PeerConnectionWrapper from which to get stats
 * @param {string} label
 *        Label for the peer connection, passed to each stats callback
 * @param {Array} operations
 *        Array of stats callbacks, each as function (stats, label)
 */
function processPcStats(pc, label, operations) {
  pc.getStats(null, function (stats) {
    operations.forEach(function (operation) {
      operation(stats, label);
    });
  });
}


/**
 * Outputs and verifies the status for local and/or remote PeerConnection as
 * appropriate
 *
 * @param {Object} test
 *        Test containing the peer connection(s) for verification
 */
function verifyConnectionStatus(test) {
  const OPERATIONS = [outputPcStats, verifyPcStats];

  if (test.pcLocal) {
    processPcStats(test.pcLocal, 'LOCAL', OPERATIONS);
  }

  if (test.pcRemote) {
    processPcStats(test.pcRemote, 'REMOTE', OPERATIONS);
  }
}


/**
 * Generates a setInterval wrapper command link for use in pc.js command chains
 *
 * The link will repeatedly call the given callback function every interval ms
 * until duration ms have passed, then it will continue the test.
 *
 * @param {function} callback
 *        Function to be called on each interval
 * @param {number} [interval=1000]
 *        Frequency in milliseconds with which callback will be called
 * @param {number} [duration=3 hours]
 *        Length of time in milliseconds for which callback will be called
 * @param {string} [name='INTERVAL_COMMAND']
 *        Name of the generated command link
 */
function generateIntervalCommand(callback, interval, duration, name) {
  interval = interval || 1000;
  duration = duration || 1000 * 3600 * 3;
  name = name || 'INTERVAL_COMMAND';

  return [
    name,
    function (test) {
      var startTime = Date.now();
      var intervalId = setInterval(function () {
        if (callback) {
          callback(test);
        }

        var failed = false;
        Object.keys(_errorCount).forEach(function (label) {
          if (_errorCount[label] > MAX_ERROR_CYCLES) {
            ok(false, "Encountered more then " + MAX_ERROR_CYCLES + " cycles" +
              " with errors on " + label);
            failed = true;
          }
        });
        var timeElapsed = Date.now() - startTime;
        if ((timeElapsed >= duration) || failed) {
          clearInterval(intervalId);
          test.next();
        }
      }, interval);
    }
  ]
}

