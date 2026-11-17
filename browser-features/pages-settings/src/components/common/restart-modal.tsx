import { useTranslation } from "react-i18next";
import { ConfirmModal } from "../../../../../libs/ui/dialog.tsx";
export function RestartModal(
  { onClose, label }: { onClose: () => void; label: string },
) {
  const { t } = useTranslation();
  const restartBrowser = () =>
    (globalThis as typeof globalThis & { NRRestartBrowser: () => void })
      .NRRestartBrowser();
  return (
    <ConfirmModal
      isOpen
      onClose={onClose}
      onConfirm={restartBrowser}
      title={t("restartWarningDialog.title")}
      confirmText={t("restartWarningDialog.restart")}
      cancelText={t("restartWarningDialog.cancel")}
    >
      <p>{label}</p>
      <p>{t("restartWarningDialog.description")}</p>
    </ConfirmModal>
  );
}
