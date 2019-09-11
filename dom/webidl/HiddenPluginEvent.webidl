interface PluginTag;

[ChromeOnly]
interface HiddenPluginEvent : Event
{
  constructor(DOMString type, optional HiddenPluginEventInit eventInit = {});

  readonly attribute PluginTag? tag;
};

dictionary HiddenPluginEventInit : EventInit
{
  PluginTag? tag = null;
};
