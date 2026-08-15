import { useCallback, useEffect, useRef } from "react";
import { useTranslation } from "react-i18next";

interface ConfirmModalProps {
  isOpen: boolean;
  onClose: () => void;
  onConfirm: () => void;
  title: string;
  children: React.ReactNode;
  confirmText?: string;
  cancelText?: string;
  confirmVariant?: string;
}

export function ConfirmModal({
  isOpen,
  onClose,
  onConfirm,
  title,
  children,
  confirmText,
  cancelText,
  confirmVariant = "btn-primary",
}: ConfirmModalProps) {
  const { t } = useTranslation();
  const dialogRef = useRef<HTMLDialogElement>(null);

  const resolvedConfirmText = confirmText ?? t("common.confirm");
  const resolvedCancelText = cancelText ?? t("common.cancel");

  useEffect(() => {
    const dialog = dialogRef.current;
    if (!dialog) return;
    if (isOpen && !dialog.open) {
      dialog.showModal();
    } else if (!isOpen && dialog.open) {
      dialog.close();
    }
  }, [isOpen]);

  // Handle ESC key and backdrop form submit — both fire the native "close" event
  const handleDialogClose = useCallback(() => {
    onClose();
  }, [onClose]);

  const handleConfirm = () => {
    onConfirm();
  };

  return (
    <dialog ref={dialogRef} className="modal" onClose={handleDialogClose}>
      <div className="modal-box">
        <h3 className="font-bold text-lg">{title}</h3>
        <div className="py-4">{children}</div>
        <div className="modal-action">
          <button type="button" className="btn btn-ghost" onClick={onClose}>
            {resolvedCancelText}
          </button>
          <button
            type="button"
            className={`btn ${confirmVariant}`}
            data-testid="notes-delete-confirm"
            onClick={handleConfirm}
          >
            {resolvedConfirmText}
          </button>
        </div>
      </div>
      <form method="dialog" className="modal-backdrop">
        <button type="submit" aria-label={t("common.close")} />
      </form>
    </dialog>
  );
}
