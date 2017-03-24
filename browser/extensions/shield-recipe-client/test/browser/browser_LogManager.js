"use strict";

Cu.import("resource://gre/modules/Log.jsm", this);
Cu.import("resource://shield-recipe-client/lib/LogManager.jsm", this);

add_task(function*() {
  // Ensure that configuring the logger affects all generated loggers.
  const firstLogger = LogManager.getLogger("first");
  LogManager.configure(5);
  const secondLogger = LogManager.getLogger("second");
  is(firstLogger.level, 5, "First logger level inherited from root logger.");
  is(secondLogger.level, 5, "Second logger level inherited from root logger.");

  // Ensure that our loggers have at least one appender.
  LogManager.configure(Log.Level.Warn);
  const logger  = LogManager.getLogger("test");
  ok(logger.appenders.length > 0, true, "Loggers have at least one appender.");

  // Ensure our loggers log to the console.
  yield new Promise(resolve => {
    SimpleTest.waitForExplicitFinish();
    SimpleTest.monitorConsole(resolve, [{message: /legend has it/}]);
    logger.warn("legend has it");
    SimpleTest.endMonitorConsole();
  });
});
