[ Func="mozilla::AddonManagerWebAPI::IsAPIEnabled",
  Constructor(DOMString type, AddonEventInit eventInitDict)]
interface AddonEvent : Event {
  readonly attribute DOMString id;
};

dictionary AddonEventInit : EventInit {
  required DOMString id;
};

