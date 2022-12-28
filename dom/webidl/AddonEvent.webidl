[Func="mozilla::AddonManagerWebAPI::IsAPIEnabled",
 Exposed=Window]
interface AddonEvent : Event {
  constructor(DOMString type, AddonEventInit eventInitDict);

  readonly attribute DOMString id;
};

dictionary AddonEventInit : EventInit {
  required DOMString id;
};
