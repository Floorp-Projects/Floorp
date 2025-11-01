/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import type { ShortcutConfig } from "../../../types/pref.ts";
import { Input } from "@/components/common/input.tsx";

interface ShortcutEditorProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: (shortcut: ShortcutConfig) => void;
    initialShortcut: ShortcutConfig | null;
    existingShortcuts: ShortcutConfig[];
    actionId: string;
}

export const ShortcutEditor = ({
    isOpen,
    onClose,
    onSave,
    initialShortcut,
    existingShortcuts,
    actionId,
}: ShortcutEditorProps) => {
    const { t } = useTranslation();
    const [shortcut, setShortcut] = useState<ShortcutConfig>(
        initialShortcut || {
            modifiers: {
                alt: false,
                ctrl: false,
                meta: false,
                shift: false,
            },
            key: "",
            action: actionId,
        }
    );
    const [isRecording, setIsRecording] = useState(false);
    const [error, setError] = useState<string | null>(null);

    const formatKeyCode = (code: string) => {
        if (!code) return "";
        return code.replace(/^(Key|Digit|Arrow)/, "");
    };

    useEffect(() => {
        const handleKeyDown = (e: KeyboardEvent) => {
            if (!e.key.startsWith("F")) {
                e.preventDefault();
            }
            e.stopPropagation();

            if (
                e.code.startsWith("Alt") ||
                e.code.startsWith("Control") ||
                e.code.startsWith("Meta") ||
                e.code.startsWith("Shift")
            ) {
                return;
            }

            const code = e.code;
            const newKey = code.replace(/^(Key|Digit)/, "");

            setShortcut((prev) => ({
                ...prev,
                key: newKey,
            }));
            setIsRecording(false);
            checkDuplicate(newKey);
        };

        if (isOpen && isRecording) {
            globalThis.addEventListener("keydown", handleKeyDown);
        }

        return () => {
            globalThis.removeEventListener("keydown", handleKeyDown);
        };
    }, [isOpen, isRecording]);

    const checkDuplicate = (key: string) => {
        const newShortcut = {
            ...shortcut,
            key,
        };

        const otherShortcuts = existingShortcuts.filter(
            (s) => s.action !== initialShortcut?.action
        );

        const isDuplicate = otherShortcuts.some((existing) => {
            return (
                existing.key === newShortcut.key &&
                existing.modifiers.alt === newShortcut.modifiers.alt &&
                existing.modifiers.ctrl === newShortcut.modifiers.ctrl &&
                existing.modifiers.meta === newShortcut.modifiers.meta &&
                existing.modifiers.shift === newShortcut.modifiers.shift
            );
        });

        if (isDuplicate) {
            setError(t("keyboardShortcut.keyConflict"));
        } else {
            setError(null);
        }
    };

    useEffect(() => {
        if (shortcut.key) {
            checkDuplicate(shortcut.key);
        }
    }, [shortcut.modifiers]);

    if (!isOpen) return null;

    const previewShortcut = () => {
        const modifiers = [];
        if (shortcut.modifiers.alt) modifiers.push("Alt");
        if (shortcut.modifiers.ctrl) modifiers.push("Ctrl");
        if (shortcut.modifiers.meta) modifiers.push("Meta");
        if (shortcut.modifiers.shift) modifiers.push("Shift");
        if (shortcut.key) modifiers.push(formatKeyCode(shortcut.key).toUpperCase());
        return modifiers.join(" + ");
    };

    return (
        <div className="modal modal-open">
            <div className="modal-box">
                <h3 className="font-bold text-lg mb-2">{t("keyboardShortcut.editShortcut")}</h3>
                <p className="text-sm text-base-content/70 mb-4">
                    {t("keyboardShortcut.modal.description")}
                </p>
                <div className="space-y-6">
                    <div className="bg-base-200 p-4 rounded-lg">
                        <div className="text-sm text-base-content/70 mb-2">
                            {t("keyboardShortcut.preview")}
                        </div>
                        <div className={`text-xl font-mono ${isRecording ? "text-primary" : ""}`}>
                            {previewShortcut() || t("keyboardShortcut.pressKey")}
                        </div>
                    </div>

                    {error && (
                        <div className="alert alert-error">
                            <span>{error}</span>
                        </div>
                    )}

                    <div className="grid grid-cols-2 gap-4">
                        <div className="flex items-center space-x-2 p-3 bg-base-200 rounded-lg">
                            <input
                                type="checkbox"
                                className="checkbox"
                                checked={shortcut.modifiers.alt}
                                onChange={(e) =>
                                    setShortcut((prev) => ({
                                        ...prev,
                                        modifiers: { ...prev.modifiers, alt: e.target.checked },
                                    }))
                                }
                            />
                            <span>Alt</span>
                        </div>
                        <div className="flex items-center space-x-2 p-3 bg-base-200 rounded-lg">
                            <input
                                type="checkbox"
                                className="checkbox"
                                checked={shortcut.modifiers.ctrl}
                                onChange={(e) =>
                                    setShortcut((prev) => ({
                                        ...prev,
                                        modifiers: { ...prev.modifiers, ctrl: e.target.checked },
                                    }))
                                }
                            />
                            <span>Ctrl</span>
                        </div>
                        <div className="flex items-center space-x-2 p-3 bg-base-200 rounded-lg">
                            <input
                                type="checkbox"
                                className="checkbox"
                                checked={shortcut.modifiers.meta}
                                onChange={(e) =>
                                    setShortcut((prev) => ({
                                        ...prev,
                                        modifiers: { ...prev.modifiers, meta: e.target.checked },
                                    }))
                                }
                            />
                            <span>Meta</span>
                        </div>
                        <div className="flex items-center space-x-2 p-3 bg-base-200 rounded-lg">
                            <input
                                type="checkbox"
                                className="checkbox"
                                checked={shortcut.modifiers.shift}
                                onChange={(e) =>
                                    setShortcut((prev) => ({
                                        ...prev,
                                        modifiers: { ...prev.modifiers, shift: e.target.checked },
                                    }))
                                }
                            />
                            <span>Shift</span>
                        </div>
                    </div>

                    <div className="form-control">
                        <label className="label">
                            <span className="label-text">{t("keyboardShortcut.key")}</span>
                        </label>
                        <div className="relative">
                            <Input
                                type="text"
                                className={`input input-bordered w-full ${isRecording ? "input-primary" : ""} ${error ? "input-error" : ""
                                    }`}
                                value={formatKeyCode(shortcut.key)}
                                readOnly
                                placeholder={t("keyboardShortcut.pressKey")}
                                onFocus={() => setIsRecording(true)}
                                onBlur={() => setIsRecording(false)}
                            />
                            {isRecording && (
                                <div className="absolute inset-0 flex items-center justify-center bg-base-200/50 rounded-lg">
                                    <span className="text-primary">{t("keyboardShortcut.recording")}</span>
                                </div>
                            )}
                        </div>
                    </div>
                </div>

                <div className="modal-action">
                    <button type="button" className="btn" onClick={onClose}>
                        {t("cancel")}
                    </button>
                    <button
                        type="button"
                        className="btn btn-primary"
                        onClick={() => {
                            onSave(shortcut);
                            onClose();
                        }}
                        disabled={!shortcut.key || !!error}
                    >
                        {t("save")}
                    </button>
                </div>
            </div>
        </div>
    );
}; 