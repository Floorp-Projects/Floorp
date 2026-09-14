import type { ConfirmModalProps } from "./types.ts";
import { Dialog, Portal } from "@chakra-ui/react";
import { useLayoutEffect, useRef } from "react";
import { Button } from "./button.tsx";
import styles from "./dialog.module.css";

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
        <Dialog.Backdrop className={styles.backdrop} />
        <Dialog.Positioner className={styles.positioner}>
          <Dialog.Content className={styles.content}>
            <Dialog.Header>
              <Dialog.Title className={styles.title}>{title}</Dialog.Title>
            </Dialog.Header>
            <Dialog.Body className={styles.body}>{children}</Dialog.Body>
            <Dialog.Footer className={styles.footer}>
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
