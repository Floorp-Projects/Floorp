============================
Set a conditional breakpoint
============================

A normal breakpoint is just associated with a line: when the program reaches that line, the debugger pauses. A conditional breakpoint also has a condition associated with it, which is represented as an `expression <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Expressions_and_Operators#expressions>`_. When the program reaches the line, the debugger pauses only if the breakpoint's specified expression evaluates to ``true``.

This makes it possible to debug specific scenarios, such as bugs that only happen on odd entries in a list, or errors that occur the last time through a loop.


To set a conditional breakpoint, activate the context menu in the :ref:`source pane <debugger_ui_tour_source_pane>`, on the line where you want the breakpoint, and select "Add Conditional Breakpoint". You'll then see a textbox where you can enter the expression. Press :kbd:`Return` to finish.

Conditional breakpoints are shown as orange arrows laid over the line number.

If you context-click on any breakpoint, you'll see a menu item "Edit Breakpoint". You can use this to modify an existing condition or to add a condition to a normal breakpoint.
