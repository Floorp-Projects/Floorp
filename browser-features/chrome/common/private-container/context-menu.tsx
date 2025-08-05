export function ContextMenu() {
  return (
    <xul:menuitem
      id="context_toggleToPrivateContainer"
      data-l10n-id="floorp-toggle-private-container"
      label="Toggle to Private Container"
      onCommand={() => {
        window.gFloorpPrivateContainer.reopenInPrivateContainer();
      }}
    />
  );
}
