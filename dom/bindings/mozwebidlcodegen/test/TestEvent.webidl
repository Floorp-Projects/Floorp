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
