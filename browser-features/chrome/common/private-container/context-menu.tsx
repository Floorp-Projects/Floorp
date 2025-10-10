import { FloorpPrivateContainer } from "./browser-private-container";

export function ContextMenu() {
  return (
    <xul:menuitem
      id="context_toggleToPrivateContainer"
      label="Toggle to Private Container"
      onCommand={() => {
        FloorpPrivateContainer.reopenInPrivateContainer();
      }}
    />
  );
}
