import type { ConfirmModalProps } from "./types.ts";
import { Dialog, Portal } from "@chakra-ui/react";
import { useLayoutEffect, useRef } from "react";
import { Button } from "./button.tsx";
import styles from "./dialog.module.css";
import { useStandardControls } from "./control-theme.ts";

export function ConfirmModal(
  {
    isOpen,
    onClose,
    onConfirm,
    title,
    children,
    confirmText = "Confirm",
    cancelText = "Cancel",
    confirmVariant = "primary",
  }: ConfirmModalProps,
) {
  const standard = useStandardControls();
  const cancelRef = useRef<HTMLButtonElement>(null);
  const openerRef = useRef<HTMLElement | null>(null);
  useLayoutEffect(() => {
    if (isOpen && document.activeElement instanceof HTMLElement) {
      openerRef.current = document.activeElement;
    }
  }, [isOpen]);
  return (
    <Dialog.Root
      open={isOpen}
      onOpenChange={({ open }) => {
        if (!open) onClose();
      }}
      role="alertdialog"
      initialFocusEl={() => cancelRef.current}
      finalFocusEl={() => openerRef.current}
      placement="center"
      lazyMount
      unmountOnExit
    >
      <Portal>
        <Dialog.Backdrop className={standard ? undefined : styles.backdrop} />
        <Dialog.Positioner className={standard ? undefined : styles.positioner}>
          <Dialog.Content
            className={standard ? "floorp-standard-ui" : styles.content}
            mx={standard ? "5" : undefined}
            maxH={standard ? "calc(100dvh - 40px)" : undefined}
            overflowY={standard ? "auto" : undefined}
          >
            <Dialog.Header>
              <Dialog.Title className={standard ? undefined : styles.title}>
                {title}
              </Dialog.Title>
            </Dialog.Header>
            <Dialog.Body className={standard ? undefined : styles.body}>
              {children}
            </Dialog.Body>
            <Dialog.Footer
              className={standard ? undefined : styles.footer}
              flexWrap={standard ? "wrap" : undefined}
            >
              <Button ref={cancelRef} onClick={onClose} variant="ghost">
                {cancelText}
              </Button>
              <Button
                onClick={() => {
                  onConfirm();
                  onClose();
                }}
                variant={confirmVariant}
              >
                {confirmText}
              </Button>
            </Dialog.Footer>
          </Dialog.Content>
        </Dialog.Positioner>
      </Portal>
    </Dialog.Root>
  );
}
