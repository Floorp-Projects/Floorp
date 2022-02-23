.. _matcher_cookbook:

Matcher Cookbook
=================

This page is designed to be a selection of common ingredients to a more complicated matcher.

.. list-table::
   :widths: 35 65
   :header-rows: 1
   :class: matcher-cookbook

   * - Desired Outcome
     - Syntax
   * - Ignore header files

       *If you have an #include in your example code, your matcher may match things in the header files.*
     - Add **isExpansionInMainFile()** to the matcher.  e.g.

       ``m functionDecl(isExpansionInMainFile())``


*More coming*
