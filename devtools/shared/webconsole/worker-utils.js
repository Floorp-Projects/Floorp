// XXXworkers This file is loaded on the server side for worker debugging.
// Since the server is running in the worker thread, it doesn't
// have access to Services / Components.  This functionality
// is stubbed out to prevent errors, and will need to implemented
// for Bug 1209353.

exports.Utils = { l10n: function() {} };
exports.ConsoleServiceListener = function() {};
exports.ConsoleAPIListener = function() {};
exports.addWebConsoleCommands = function() {};
exports.JSPropertyProvider = function() {};
exports.ConsoleReflowListener = function() {};
exports.CONSOLE_WORKER_IDS = [];
