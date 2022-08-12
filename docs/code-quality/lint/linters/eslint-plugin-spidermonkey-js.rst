==============================
Mozilla ESLint SpiderMonkey JS
==============================

This plugin adds a processor and an environment for the SpiderMonkey JS code.

Processors
==========

The processor is used to pre-process all `*.js` files and deals with the macros
that SpiderMonkey uses.

Environments
============

The plugin provides a custom environment for SpiderMonkey's self-hosted code. It
adds all self-hosting functions, error message numbers, and other self-hosting
definitions as global, read-only identifiers.
