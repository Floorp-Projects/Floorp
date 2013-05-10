/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

const SERVER_PORT = 12345;
const DATA_ARRAY = [0, 255, 254, 0, 1, 2, 3, 0, 255, 255, 254, 0];

function UDPSocketInternalImpl() {
}

UDPSocketInternalImpl.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUDPSocketInternal]),
  callListenerError: function(type, message, filename, lineNumber, columnNumber) {
    if (this.onerror) {
      this.onerror();
    } else {
      do_throw('Received unexpected error: ' + message + ' at ' + filename +
                ':' + lineNumber + ':' + columnNumber);
    }
  },
  callListenerReceivedData: function(type, host, port, data, dataLength) {
    do_print('*** recv data(' + dataLength + ')=' + data.join() + '\n');
    if (this.ondata) {
      try {
        this.ondata(data, dataLength);
      } catch(ex) {
        if (ex === Cr.NS_ERROR_ABORT)
          throw ex;
        do_print('Caught exception: ' + ex + '\n' + ex.stack);
        do_throw('test is broken; bad ondata handler; see above');
      }
    } else {
      do_throw('Received ' + dataLength + ' bytes of unexpected data!');
    }
  },
  callListenerVoid: function(type) {
    switch (type) {
    case 'onopen':
      if (this.onopen) {
        this.onopen();
      }
      break;
    case 'onclose':
      if (this.onclose) {
        this.onclose();
      }
      break;
    }
  },
  callListenerSent: function(type, value) {
    if (value != Cr.NS_OK) {
      do_throw('Previous send was failed with cause: ' + value);
    }
  },
  updateReadyState: function(readyState) {
    do_print('*** current state: ' + readyState + '\n');
  },
  onopen: function() {},
  onclose: function() {},
};

function makeSuccessCase(name) {
  return function() {
    do_print('got expected: ' + name);
    run_next_test();
  };
}

function makeJointSuccess(names) {
  let funcs = {}, successCount = 0;
  names.forEach(function(name) {
    funcs[name] = function() {
      do_print('got excepted: ' + name);
      if (++successCount === names.length)
        run_next_test();
    };
  });
  return funcs;
}

function makeExpectedData(expectedData, callback) {
  return function(receivedData, receivedDataLength) {
    if (receivedDataLength != expectedData.length) {
      do_throw('Received data size mismatched, expected ' + expectedData.length +
               ' but got ' + receivedDataLength);
    }
    for (let i = 0; i < receivedDataLength; i++) {
      if (receivedData[i] != expectedData[i]) {
        do_throw('Received mismatched data at position ' + i);
      }
    }
    if (callback) {
      callback();
    } else {
      run_next_test();
    }
  };
}

function makeFailureCase(name) {
  return function() {
    let argstr;
    if (arguments.length) {
      argstr = '(args: ' +
        Array.map(arguments, function(x) {return x.data + ""; }).join(" ") + ')';
    } else {
      argstr = '(no arguments)';
    }
    do_throw('got unexpected: ' + name + ' ' + argstr);
  };
}

function createSocketChild() {
  return Cc['@mozilla.org/udp-socket-child;1']
           .createInstance(Ci.nsIUDPSocketChild);
}

var UDPSocket = createSocketChild();
var callback = new UDPSocketInternalImpl();

function connectSock() {
  UDPSocket.bind(callback, '127.0.0.1', SERVER_PORT);
  callback.onopen = makeSuccessCase('open');
}

function sendData() {
  UDPSocket.send('127.0.0.1', SERVER_PORT, DATA_ARRAY, DATA_ARRAY.length);
  callback.ondata = makeExpectedData(DATA_ARRAY);
}

function clientClose() {
  UDPSocket.close();
  callback.ondata = makeFailureCase('data');
  callback.onclose = makeSuccessCase('close');
}

function connectError() {
  UDPSocket = createSocketChild();
  UDPSocket.bind(callback, 'some non-IP string', SERVER_PORT);
  callback.onerror = makeSuccessCase('error');
  callback.onopen = makeFailureCase('open');
}

function cleanup() {
  UDPSocket = null;
  run_next_test();
}

add_test(connectSock);
add_test(sendData);
add_test(clientClose);
add_test(connectError);
add_test(cleanup);

function run_test() {
  run_next_test();
}
