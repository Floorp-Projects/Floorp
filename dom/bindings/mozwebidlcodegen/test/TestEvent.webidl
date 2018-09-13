/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

interface EventTarget {
  void addEventListener();
};

interface Event {};

callback EventHandlerNonNull = any (Event event);
typedef EventHandlerNonNull? EventHandler;

[NoInterfaceObject]
interface TestEvent : EventTarget {
  attribute EventHandler onfoo;
};
