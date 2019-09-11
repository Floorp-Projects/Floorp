[Func="mozilla::AddonManagerWebAPI::IsAPIEnabled"]
interface AddonEvent : Event {
  constructor(DOMString type, AddonEventInit eventInitDict);

  readonly attribute DOMString id;
};

dictionary AddonEventInit : EventInit {
  required DOMString id;
};

