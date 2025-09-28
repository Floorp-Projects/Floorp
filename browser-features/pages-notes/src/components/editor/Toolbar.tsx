import { useCallback, useEffect, useState } from "react";
import { useLexicalComposerContext } from "@lexical/react/LexicalComposerContext";
import {
    $getSelection,
    $isRangeSelection,
    FORMAT_TEXT_COMMAND,
    FORMAT_ELEMENT_COMMAND,
    $createParagraphNode,
    $isElementNode,
} from "lexical";
import {
    $createHeadingNode,
    $isHeadingNode,
    type HeadingTagType,
} from "@lexical/rich-text";
import {
    $isListNode,
    INSERT_UNORDERED_LIST_COMMAND,
    INSERT_ORDERED_LIST_COMMAND,
} from "@lexical/list";
import { $setBlocksType } from "@lexical/selection";
import { useTranslation } from "react-i18next";
import {
    Bold,
    Italic,
    Underline,
    Strikethrough,
    List,
    ListOrdered,
    AlignLeft,
    AlignCenter,
    AlignRight,
    Heading1,
    Heading2,
    Heading3,
} from "lucide-react";

export const Toolbar = () => {
    const { t } = useTranslation();
    const [editor] = useLexicalComposerContext();
    const [activeFormats, setActiveFormats] = useState<{
        bold: boolean;
        italic: boolean;
        underline: boolean;
        strikethrough: boolean;
        list: "bullet" | "number" | null;
        align: "left" | "center" | "right" | null;
        heading: HeadingTagType | null;
    }>({
        bold: false,
        italic: false,
        underline: false,
        strikethrough: false,
        list: null,
        align: null,
        heading: null,
    });

    useEffect(() => {
        return editor.registerUpdateListener(({ editorState }) => {
            editorState.read(() => {
                const selection = $getSelection();
                if (!$isRangeSelection(selection)) {
                    setActiveFormats({
                        bold: false,
                        italic: false,
                        underline: false,
                        strikethrough: false,
                        list: null,
                        align: null,
                        heading: null,
                    });
                    return;
                }

                const anchorNode = selection.anchor.getNode();
                const element =
                    anchorNode.getKey() === "root"
                        ? anchorNode
                        : anchorNode.getTopLevelElementOrThrow();

                const listNode = $isListNode(element) ? element : null;
                const listType = listNode ? listNode.getListType() : null;

                const headingNode = $isHeadingNode(element) ? element : null;
                const headingType = headingNode ? headingNode.getTag() : null;

                const isBold = selection.hasFormat("bold");
                const isItalic = selection.hasFormat("italic");
                const isUnderline = selection.hasFormat("underline");
                const isStrikethrough = selection.hasFormat("strikethrough");

                const align = $isElementNode(element) ? element.getFormatType() : null;

                setActiveFormats({
                    bold: isBold,
                    italic: isItalic,
                    underline: isUnderline,
                    strikethrough: isStrikethrough,
                    list:
                        listType === "bullet" || listType === "number" ? listType : null,
                    align: align as "left" | "center" | "right" | null,
                    heading: headingType,
                });
            });
        });
    }, [editor]);

    const formatText = useCallback(
        (command: "bold" | "italic" | "underline" | "strikethrough") => {
            editor.update(() => {
                const selection = $getSelection();
                if (!$isRangeSelection(selection)) {
                    return;
                }

                if (command === "underline") {
                    if (selection.hasFormat("strikethrough")) {
                        editor.dispatchCommand(FORMAT_TEXT_COMMAND, "strikethrough");
                    }
                    editor.dispatchCommand(FORMAT_TEXT_COMMAND, "underline");
                } else if (command === "strikethrough") {
                    if (selection.hasFormat("underline")) {
                        editor.dispatchCommand(FORMAT_TEXT_COMMAND, "underline");
                    }
                    editor.dispatchCommand(FORMAT_TEXT_COMMAND, "strikethrough");
                } else {
                    editor.dispatchCommand(FORMAT_TEXT_COMMAND, command);
                }
            });
        },
        [editor],
    );

    const formatElement = useCallback(
        (command: "left" | "center" | "right") => {
            editor.update(() => {
                const selection = $getSelection();
                if ($isRangeSelection(selection)) {
                    editor.dispatchCommand(FORMAT_ELEMENT_COMMAND, command);
                }
            });
        },
        [editor],
    );

    const formatHeading = useCallback(
        (type: HeadingTagType) => {
            editor.update(() => {
                const selection = $getSelection();
                if (!$isRangeSelection(selection)) return;

                const element = selection.anchor.getNode().getTopLevelElementOrThrow();
                const isHeading = $isHeadingNode(element);
                const currentType = isHeading ? element.getTag() : null;

                if (isHeading && currentType === type) {
                    $setBlocksType(selection, () => $createParagraphNode());
                } else {
                    $setBlocksType(selection, () => $createHeadingNode(type));
                }
            });
        },
        [editor],
    );

    const toggleUnOrderList = useCallback(() => {
        editor.update(() => {
            const selection = $getSelection();
            if (!$isRangeSelection(selection)) return;

            const element = selection.anchor.getNode().getTopLevelElementOrThrow();
            if ($isListNode(element) && element.getListType() === 'bullet') {
                $setBlocksType(selection, () => $createParagraphNode());
            } else {
                editor.dispatchCommand(INSERT_UNORDERED_LIST_COMMAND, undefined);
            }
        });
    }, [editor]);

    const toggleOrderList = useCallback(() => {
        editor.update(() => {
            const selection = $getSelection();
            if (!$isRangeSelection(selection)) return;

            const element = selection.anchor.getNode().getTopLevelElementOrThrow();
            if ($isListNode(element) && element.getListType() === 'number') {
                $setBlocksType(selection, () => $createParagraphNode());
            } else {
                editor.dispatchCommand(INSERT_ORDERED_LIST_COMMAND, undefined);
            }
        });
    }, [editor]);

    return (
        <>
            <div className="flex flex-wrap gap-0.5 p-1">
                <div className="flex flex-wrap gap-0.5">
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.heading === "h1" ? "btn-active" : "btn-ghost"}`}
                        onClick={() => formatHeading("h1")}
                        aria-label={t("editor.heading1")}
                    >
                        <Heading1 className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.heading === "h2" ? "btn-active" : "btn-ghost"}`}
                        onClick={() => formatHeading("h2")}
                        aria-label={t("editor.heading2")}
                    >
                        <Heading2 className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.heading === "h3" ? "btn-active" : "btn-ghost"}`}
                        onClick={() => formatHeading("h3")}
                        aria-label={t("editor.heading3")}
                    >
                        <Heading3 className="h-4 w-4" />
                    </button>
                </div>
                <div className="divider divider-horizontal mx-0"></div>
                <div className="flex flex-wrap gap-0.5">
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.bold ? "btn-active" : "btn-ghost"}`}
                        onClick={() => formatText("bold")}
                        aria-label={t("editor.bold")}
                    >
                        <Bold className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.italic ? "btn-active" : "btn-ghost"}`}
                        onClick={() => formatText("italic")}
                        aria-label={t("editor.italic")}
                    >
                        <Italic className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.underline ? "btn-active" : "btn-ghost"}`}
                        onClick={() => formatText("underline")}
                        aria-label={t("editor.underline")}
                    >
                        <Underline className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.strikethrough ? "btn-active" : "btn-ghost"}`}
                        onClick={() => formatText("strikethrough")}
                        aria-label={t("editor.strikethrough")}
                    >
                        <Strikethrough className="h-4 w-4" />
                    </button>
                </div>
                <div className="flex flex-wrap gap-0.5">
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.list === "bullet" ? "btn-active" : "btn-ghost"}`}
                        onClick={toggleUnOrderList}
                        aria-label={t("editor.bulletList")}
                    >
                        <List className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.list === "number" ? "btn-active" : "btn-ghost"}`}
                        onClick={toggleOrderList}
                        aria-label={t("editor.numberedList")}
                    >
                        <ListOrdered className="h-4 w-4" />
                    </button>
                </div>
                <div className="divider divider-horizontal mx-0"></div>
                <button
                    type="button"
                    className={`btn btn-sm ${activeFormats.align === "left" ? "btn-active" : "btn-ghost"}`}
                    onClick={() => formatElement("left")}
                    aria-label={t("editor.alignLeft")}
                >
                    <AlignLeft className="h-4 w-4" />
                </button>
                <button
                    type="button"
                    className={`btn btn-sm ${activeFormats.align === "center" ? "btn-active" : "btn-ghost"}`}
                    onClick={() => formatElement("center")}
                    aria-label={t("editor.alignCenter")}
                >
                    <AlignCenter className="h-4 w-4" />
                </button>
                <button
                    type="button"
                    className={`btn btn-sm ${activeFormats.align === "right" ? "btn-active" : "btn-ghost"}`}
                    onClick={() => formatElement("right")}
                    aria-label={t("editor.alignRight")}
                >
                    <AlignRight className="h-4 w-4" />
                </button>
            </div>
            <div className="divider my-0"></div>
        </>
    );
};
