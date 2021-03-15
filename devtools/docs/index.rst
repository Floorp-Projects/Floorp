=================================
Firefox DevTools Contributor Docs
=================================

This is a guide to working on the code for Firefox Developer Tools. If you're looking for help with using the tools, see the `user docs <https://developer.mozilla.org/en-US/docs/Tools>`__. For other ways to get involved, check out our `community site <https://firefox-dev.tools/>`__.


Getting Started
===============
.. toctree::
   :maxdepth: 1

   Getting Started <getting-started/README.md>
   Get a Bugzilla account <getting-started/bugzilla.md>
   Create a development profile <getting-started/development-profiles.md>


Contributing
============
.. toctree::
   :maxdepth: 1

   Contributing <contributing.md>
   Find bugs to work on <contributing/find-bugs.md>
   How to fix a bug <contributing/fixing-bugs.md>
   Code reviews <contributing/code-reviews.md>
   Landing code <contributing/landing-code.md>
   Leveling up <contributing/levelling-up.md>
   Coding standards <contributing/coding-standards.md>
   Filing good bugs <contributing/filing-good-bugs.md>
   Investigating performance issues <contributing/performance.md>
   Writing efficient React code <contributing/react-performance-tips.md>


Automated tests
===============
.. toctree::
   :maxdepth: 1

   Automated tests <tests/README.md>
   xpcshell <tests/xpcshell.md>
   Chrome mochitests <tests/mochitest-chrome.md>
   DevTools mochitests <tests/mochitest-devtools.md>
   Node tests <tests/node-tests.md>
   Writing tests <tests/writing-tests.md>
   Debugging intermittent failures <tests/debugging-intermittents.md>
   Performance tests (DAMP) <tests/performance-tests.md>
   Writing a new test <tests/writing-perf-tests.md>
   Example <tests/writing-perf-tests-example.md>
   Advanced tips <tests/writing-perf-tests-tips.md>

Files and directories
=====================
.. toctree::
   :maxdepth: 1

   Files and directories <files/README.md>
   Adding New Files <files/adding-files.md>


Tool Architectures
==================
.. toctree::
   :maxdepth: 1

   Inspector Panel Architecture <tools/inspector-panel.md>
   Inspector Highlighters <tools/highlighters.md>
   Memory <tools/memory-panel.md>
   Debugger <tools/debugger-panel.md>
   Responsive Design Mode <tools/responsive-design-mode.md>
   Console <tools/console-panel.md>
   Network </devtools/netmonitor/architecture.md>


Frontend
========
.. toctree::
   :maxdepth: 1

  Panel SVGs <frontend/svgs.md>
  React <frontend/react.md>
  React Guidelines <frontend/react-guidelines.md>
  Redux <frontend/redux.md>
  Redux Guidelines <frontend/redux-guidelines.md>
  Telemetry <frontend/telemetry.md>
  Content Security Policy <frontend/csp.md>


Backend
=======
.. toctree::
   :maxdepth: 1

   Remote Debugging Protocol <backend/protocol.md>
   Client API <backend/client-api.md>
   Debugger API <backend/debugger-api.md>
   Backward Compatibility <backend/backward-compatibility.md>
   Actors Organization <backend/actor-hierarchy.md>
   Handling Multi-Processes in Actors <backend/actor-e10s-handling.md>
   Writing Actors With protocol.js <backend/protocol.js.md>
   Registering A New Actor <backend/actor-registration.md>
   Actor Best Practices <backend/actor-best-practices.md>

Preferences
===========
.. toctree::
   :maxdepth: 1

   Preferences <preferences.md>
