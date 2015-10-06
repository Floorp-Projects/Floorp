var {CustomizableUI} = Cu.import("resource:///modules/CustomizableUI.jsm");

function makeWidgetId(id)
{
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}
