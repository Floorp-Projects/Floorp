import { Dialog, Portal } from "@chakra-ui/react";
import { Button } from "./button.tsx";
import type { ModalProps } from "./types.ts";
import styles from "./dialog.module.css";

export function Modal(
  { title, children, onClose, closeLabel, closeOnEscape = true, wide = false }:
    ModalProps,
) {
  return (
    <Dialog.Root
      open
      onOpenChange={({ open }) => {
        if (!open) onClose();
      }}
      closeOnEscape={closeOnEscape}
      closeOnInteractOutside={false}
    >
      <Portal>
        <Dialog.Backdrop className={styles.backdrop} />
        <Dialog.Positioner className={styles.positioner}>
          <Dialog.Content className={styles.content} data-wide={wide}>
            <Dialog.Header className={styles.header}>
              <Dialog.Title className={styles.title}>{title}</Dialog.Title>
              <Button variant="ghost" aria-label={closeLabel} onClick={onClose}>
                ×
              </Button>
            </Dialog.Header>
            <Dialog.Body className={styles.body}>{children}</Dialog.Body>
          </Dialog.Content>
        </Dialog.Positioner>
      </Portal>
    </Dialog.Root>
  );
}
