interface PluginTag;

[ChromeOnly,
 Exposed=Window]
interface HiddenPluginEvent : Event
{
  constructor(DOMString type, optional HiddenPluginEventInit eventInit = {});

  readonly attribute PluginTag? tag;
};

dictionary HiddenPluginEventInit : EventInit
{
  PluginTag? tag = null;
};
