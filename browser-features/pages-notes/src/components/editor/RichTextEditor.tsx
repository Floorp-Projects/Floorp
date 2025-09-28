import { LexicalComposer } from "@lexical/react/LexicalComposer";
import { ContentEditable } from "@lexical/react/LexicalContentEditable";
import { editorConfig } from "./config.ts";
import { Toolbar } from "./Toolbar.tsx";

import { RichTextPlugin } from "@lexical/react/LexicalRichTextPlugin";
import { HistoryPlugin } from "@lexical/react/LexicalHistoryPlugin";
import { OnChangePlugin } from "@lexical/react/LexicalOnChangePlugin";
import { ListPlugin } from "@lexical/react/LexicalListPlugin";
import { CheckListPlugin } from "@lexical/react/LexicalCheckListPlugin";
import { SerializedEditorState, SerializedLexicalNode } from "lexical";
import { useEffect, useRef } from "react";
import { useLexicalComposerContext } from "@lexical/react/LexicalComposerContext";
import { $getRoot, $createParagraphNode, $createTextNode } from "lexical";

interface RichTextEditorProps {
    onChange: (editorState: SerializedEditorState<SerializedLexicalNode>) => void;
    initialContent?: string;
}

export const RichTextEditor = ({ onChange, initialContent }: RichTextEditorProps) => {
    return (
        <LexicalComposer initialConfig={editorConfig}>
            <EditorContent onChange={onChange} initialContent={initialContent} />
        </LexicalComposer>
    );
};

const EditorContent = ({ onChange, initialContent }: RichTextEditorProps) => {
    const [editor] = useLexicalComposerContext();
    const isInitialized = useRef(false);

    useEffect(() => {
        if (initialContent && !isInitialized.current) {
            try {
                const parsedContent = JSON.parse(initialContent);
                editor.setEditorState(editor.parseEditorState(parsedContent));
            } catch (e) {
                console.log("Failed to parse initial content:", e);
                editor.update(() => {
                    const root = $getRoot();
                    root.clear();

                    const lines = initialContent.split("\n");

                    for (const line of lines) {
                        const paragraphNode = $createParagraphNode();
                        if (line.length > 0) {
                            paragraphNode.append($createTextNode(line));
                        }
                        root.append(paragraphNode);
                    }
                });
            }
            isInitialized.current = true;
        }
    }, [editor, initialContent]);

    return (
        <div className="flex flex-col h-full">
            <Toolbar />
            <div className="flex-1 overflow-auto p-4">
                <RichTextPlugin
                    contentEditable={
                        <ContentEditable className="h-full outline-none" />
                    }
                    ErrorBoundary={({ children }) => children}
                />
            </div>
            {onChange && (
                <OnChangePlugin
                    onChange={(editorState) => {
                        editorState.read(() => {
                            const content = editorState.toJSON();
                            onChange(content);
                        });
                    }}
                />
            )}
            <ListPlugin />
            <CheckListPlugin />
            <HistoryPlugin />
        </div>
    );
};