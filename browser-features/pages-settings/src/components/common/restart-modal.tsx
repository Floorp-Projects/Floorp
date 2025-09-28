import { useTranslation } from "react-i18next";
import { useEffect, useRef } from "react";
import { MessageCircleWarningIcon } from "lucide-react/icons";

// WindowにNRRestartBrowserメソッドを追加するための型拡張
declare global {
    interface Window {
        NRRestartBrowser: () => void;
    }
}

export function RestartModal(props: { onClose: () => void, label: string }) {
    const { t } = useTranslation();
    const dialogRef = useRef<HTMLDialogElement>(null);

    useEffect(() => {
        if (dialogRef.current) {
            dialogRef.current.showModal();
        }
    }, []);

    const handleRestartLater = () => {
        props.onClose();
    };

    const handleRestart = () => {
        props.onClose();
        window.NRRestartBrowser();
    };

    return (
        <dialog
            ref={dialogRef}
            className="modal"
        >
            <div className="modal-box">
                <h3 className="font-bold text-lg">{t("restartWarningDialog.title")}</h3>

                <p className="my-2">
                    {props.label}
                </p>

                <div role="alert" className="alert alert-warning alert-soft my-4">
                    <MessageCircleWarningIcon className="size-6" />
                    <span>{t("restartWarningDialog.description")}</span>
                </div>

                <div className="modal-action">
                    <button
                        type="button"
                        className="btn"
                        onClick={handleRestartLater}
                    >
                        {t("restartWarningDialog.cancel")}
                    </button>
                    <button
                        type="button"
                        className="btn btn-primary"
                        onClick={handleRestart}
                    >
                        {t("restartWarningDialog.restart")}
                    </button>
                </div>
            </div>
            <form method="dialog" className="modal-backdrop">
                <button
                    type="button"
                    onClick={handleRestartLater}
                >
                    {t("common.close")}
                </button>
            </form>
        </dialog>
    );
}
