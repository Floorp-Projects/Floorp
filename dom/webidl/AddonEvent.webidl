[ Func="mozilla::AddonManagerWebAPI::IsAPIEnabled",
  Constructor(DOMString type, AddonEventInit eventInitDict)]
interface AddonEvent : Event {
  readonly attribute DOMString id;
  readonly attribute boolean needsRestart;
};

dictionary AddonEventInit : EventInit {
  required DOMString id;
  required boolean needsRestart;
};

