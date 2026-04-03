import { useEditor, EditorContent, ReactNodeViewRenderer, type JSONContent } from "@tiptap/react";
import StarterKit from "@tiptap/starter-kit";
import Underline from "@tiptap/extension-underline";
import TextAlign from "@tiptap/extension-text-align";
import Placeholder from "@tiptap/extension-placeholder";
import Image from "@tiptap/extension-image";
import { useTranslation } from "react-i18next";
import { useMemo } from "react";
import { ResizableImage } from "./ResizableImage.tsx";
import { Toolbar } from "./Toolbar.tsx";
import { Selection as PmSelection } from "@tiptap/pm/state";
import { migrateLexicalContent } from "../../lib/migrateLexicalToTiptap.ts";
import { compressImage } from "../../lib/imageCompressor.ts";

const ResizableImageExtension = Image.extend({
    addAttributes() {
        return {
            ...this.parent?.(),
            width: {
                default: null,
                renderHTML: (attributes: Record<string, unknown>) => {
                    if (!attributes.width) return {};
                    return { width: attributes.width };
                },
            },
        };
    },
    addNodeView() {
        return ReactNodeViewRenderer(ResizableImage);
    },
});

interface RichTextEditorProps {
    onChange: (json: JSONContent) => void;
    initialContent?: string;
}

function insertCompressedImage(
    // deno-lint-ignore no-explicit-any
    view: { state: { schema: any; tr: any }; dispatch: (tr: any) => void },
    file: File | Blob,
) {
    compressImage(file).then((dataUrl) => {
        const node = view.state.schema.nodes.image?.create({ src: dataUrl });
        if (node) {
            view.dispatch(view.state.tr.replaceSelectionWith(node));
        }
    }).catch((err) => {
        console.error("Failed to insert image:", err);
    });
}

export const RichTextEditor = ({ onChange, initialContent }: RichTextEditorProps) => {
    const { t } = useTranslation();

    const parsedContent = useMemo(
        () => migrateLexicalContent(initialContent),
        // initialContent is only used on mount (component is keyed by note id)
        [],
    );

    const editor = useEditor({
        extensions: [
            StarterKit,
            Underline,
            TextAlign.configure({ types: ["heading", "paragraph"] }),
            Placeholder.configure({ placeholder: t("editor.placeholder") }),
            ResizableImageExtension.configure({ allowBase64: true, inline: false }),
        ],
        content: parsedContent,
        onUpdate: ({ editor }) => {
            onChange(editor.getJSON());
        },
        editorProps: {
            attributes: {
                "aria-multiline": "true",
                spellcheck: "false",
            },
            handleKeyDown: (view, event) => {
                // In chrome:// context (production sidebar), the XUL <browser>
                // element runs without type="content", so Firefox's nsIFocusManager
                // intercepts arrow key default actions and moves focus to adjacent
                // elements. stopPropagation() is ineffective because nsIFocusManager
                // operates outside DOM event propagation.
                //
                // Fix: consume the event and manually move the cursor using
                // Selection.modify(), which manipulates the DOM selection directly
                // without triggering nsIFocusManager's focus navigation.
                if (
                    !event.ctrlKey && !event.metaKey && !event.altKey &&
                    (event.key === "ArrowUp" || event.key === "ArrowDown" ||
                     event.key === "ArrowLeft" || event.key === "ArrowRight")
                ) {
                    const sel = view.dom.ownerDocument.getSelection();
                    if (sel?.modify) {
                        const alter = event.shiftKey ? "extend" : "move";
                        const vertical = event.key === "ArrowUp" || event.key === "ArrowDown";
                        const direction = event.key === "ArrowUp" || event.key === "ArrowLeft"
                            ? "backward" : "forward";
                        const granularity = vertical ? "line" : "character";
                        sel.modify(alter, direction, granularity);
                    }
                    return true;
                }
                return false;
            },
            handlePaste: (view, event) => {
                const items = event.clipboardData?.items;
                if (!items) return false;
                for (const item of Array.from(items)) {
                    if (item.type.startsWith("image/")) {
                        const file = item.getAsFile();
                        if (file) {
                            insertCompressedImage(view, file);
                            return true;
                        }
                    }
                }
                return false;
            },
            handleDrop: (view, event) => {
                const files = event.dataTransfer?.files;
                if (!files?.length) return false;
                for (const file of Array.from(files)) {
                    if (file.type.startsWith("image/")) {
                        // Resolve drop position from cursor coordinates
                        const dropPos = view.posAtCoords({ left: event.clientX, top: event.clientY });
                        if (dropPos) {
                            const resolved = view.state.doc.resolve(dropPos.pos);
                            const tr = view.state.tr.setSelection(
                                PmSelection.near(resolved),
                            );
                            view.dispatch(tr);
                        }
                        insertCompressedImage(view, file);
                        return true;
                    }
                }
                return false;
            },
        },
    });

    return (
        <div className="flex flex-col h-full">
            <Toolbar editor={editor} />
            <div className="flex-1 overflow-auto p-2">
                <EditorContent
                    editor={editor}
                    className="h-full"
                    aria-label={t("editor.contentArea")}
                />
            </div>
        </div>
    );
};
