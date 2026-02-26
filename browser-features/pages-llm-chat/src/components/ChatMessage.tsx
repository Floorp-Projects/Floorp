import { useEffect, useMemo, useState } from "react";
import {
  Bot,
  CheckCircle2,
  ChevronRight,
  Copy,
  Sparkles,
  ThumbsDown,
  ThumbsUp,
  User,
  Wrench,
  XCircle,
} from "lucide-react";
import type { Message, ToolRun } from "../types.ts";
import ReactMarkdown from "react-markdown";
import { Prism as SyntaxHighlighter } from "react-syntax-highlighter";
import { oneDark } from "react-syntax-highlighter/dist/esm/styles/prism/index.js";

interface ChatMessageProps {
  message: Message;
  isStreaming?: boolean;
}

function formatTime(timestamp: number): string {
  try {
    return new Intl.DateTimeFormat(undefined, {
      hour: "2-digit",
      minute: "2-digit",
    }).format(timestamp);
  } catch {
    return "";
  }
}

function truncate(value: string, max = 200): string {
  if (value.length <= max) return value;
  return `${value.slice(0, max - 1)}…`;
}

function toolStatusIcon(run: ToolRun) {
  switch (run.status) {
    case "running":
      return (
        <span className="relative flex h-3.5 w-3.5">
          <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-primary opacity-75">
          </span>
          <span className="relative inline-flex rounded-full h-3.5 w-3.5 bg-primary">
          </span>
        </span>
      );
    case "error":
      return <XCircle size={14} className="text-error" />;
    default:
      return <CheckCircle2 size={14} className="text-success" />;
  }
}

export function ChatMessage(
  { message, isStreaming = false }: ChatMessageProps,
) {
  const isUser = message.role === "user";
  const isAssistant = message.role === "assistant";
  const toolRuns = message.toolRuns ?? [];
  const hasRunningTools = toolRuns.some((r) => r.status === "running");
  const [toolsExpanded, setToolsExpanded] = useState(hasRunningTools);

  useEffect(() => {
    if (hasRunningTools) setToolsExpanded(true);
  }, [hasRunningTools]);

  const formattedTime = useMemo(
    () => formatTime(message.timestamp),
    [message.timestamp],
  );

  const copyMessage = async () => {
    if (!message.content) return;
    try {
      await navigator.clipboard.writeText(message.content);
    } catch {
      /* noop */
    }
  };

  /* ── User turn ────────────────────────────────────────── */
  if (isUser) {
    return (
      <article className="group border-b border-base-200 py-4 first:pt-0">
        {/* Header row */}
        <div className="mb-2 flex items-center gap-2">
          <div className="flex h-6 w-6 shrink-0 items-center justify-center rounded-md bg-base-content/10 text-base-content/70">
            <User size={14} />
          </div>
          <span className="text-[13px] font-semibold text-base-content">
            You
          </span>
          {formattedTime && (
            <time className="text-[11px] text-base-content/35">
              {formattedTime}
            </time>
          )}
        </div>
        {/* Body */}
        <div className="pl-8 text-[13px] leading-relaxed text-base-content whitespace-pre-wrap wrap-break-word">
          {message.content || "…"}
        </div>
      </article>
    );
  }

  /* ── Assistant / system turn ──────────────────────────── */
  return (
    <article className="group border-b border-base-200 py-4 first:pt-0">
      {/* Header row */}
      <div className="mb-2 flex items-center gap-2">
        <div className="flex h-6 w-6 shrink-0 items-center justify-center rounded-md bg-primary/15 text-primary">
          {isAssistant ? <Sparkles size={14} /> : <Bot size={14} />}
        </div>
        <span className="text-[13px] font-semibold text-base-content">
          {isAssistant ? "Floorp Agent" : message.role}
        </span>
        {message.provider && (
          <span className="text-[11px] font-mono text-base-content/40 uppercase">
            {message.provider}
          </span>
        )}
        {message.model && (
          <span className="text-[11px] font-mono text-base-content/30">
            {message.model}
          </span>
        )}
        {formattedTime && (
          <time className="text-[11px] text-base-content/35">
            {formattedTime}
          </time>
        )}
        {isStreaming && (
          <span className="ml-1 inline-flex items-center gap-1 text-[11px] text-base-content/50">
            <span className="loading loading-dots loading-xs" />
          </span>
        )}
      </div>

      <div className="pl-8 space-y-2">
        {/* Tool runs — VS Code style: compact, inline */}
        {toolRuns.length > 0 && (
          <div className="text-[12px]">
            <button
              type="button"
              onClick={() => setToolsExpanded((v) => !v)}
              className="inline-flex items-center gap-1.5 text-base-content/50 hover:text-base-content/70 transition-colors"
            >
              <ChevronRight
                size={12}
                className={`transition-transform duration-150 ${
                  toolsExpanded ? "rotate-90" : ""
                }`}
              />
              <Wrench size={12} />
              <span>
                Used {toolRuns.length} tool{toolRuns.length === 1 ? "" : "s"}
              </span>
              {hasRunningTools && (
                <span className="loading loading-spinner loading-xs ml-1" />
              )}
            </button>

            {toolsExpanded && (
              <div className="mt-1 ml-0.5 border-l-2 border-base-300 pl-3 space-y-1">
                {toolRuns.map((run) => (
                  <div key={run.id} className="flex items-start gap-1.5">
                    <span className="mt-0.5 shrink-0">
                      {toolStatusIcon(run)}
                    </span>
                    <div className="min-w-0 flex-1">
                      <span className="font-medium text-base-content/70">
                        {run.name}
                      </span>
                      {run.finishedAt && (
                        <span className="ml-1.5 text-[10px] text-base-content/30">
                          {Math.max(1, run.finishedAt - run.startedAt)}ms
                        </span>
                      )}
                      {run.resultPreview && (
                        <div
                          className={`mt-0.5 text-[11px] leading-4 truncate max-w-full ${
                            run.status === "error"
                              ? "text-error/80"
                              : "text-base-content/40"
                          }`}
                          title={run.resultPreview}
                        >
                          {truncate(run.resultPreview, 120)}
                        </div>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}

        {/* Message content — no bubble, plain text */}
        <div className="markdown-content text-[13px] leading-relaxed text-base-content">
          <ReactMarkdown
            components={{
              code({ className, children, ...props }) {
                const match = /language-(\w+)/.exec(className || "");
                const isInline = !match;

                if (isInline) {
                  return (
                    <code
                      className="rounded-sm bg-base-300/70 px-1 py-0.5 text-[12px] font-mono"
                      {...props}
                    >
                      {children}
                    </code>
                  );
                }

                return (
                  <SyntaxHighlighter
                    style={oneDark}
                    language={match[1]}
                    PreTag="div"
                    className="my-2! overflow-x-auto rounded-md! text-[12px]!"
                  >
                    {String(children).replace(/\n$/, "")}
                  </SyntaxHighlighter>
                );
              },
              table({ children, ...props }) {
                return (
                  <div className="overflow-x-auto my-2">
                    <table
                      className="min-w-full border-collapse border border-base-300 text-[12px]"
                      {...props}
                    >
                      {children}
                    </table>
                  </div>
                );
              },
              thead({ children, ...props }) {
                return (
                  <thead className="bg-base-200" {...props}>
                    {children}
                  </thead>
                );
              },
              tbody({ children, ...props }) {
                return <tbody {...props}>{children}</tbody>;
              },
              tr({ children, ...props }) {
                return (
                  <tr className="border-b border-base-300" {...props}>
                    {children}
                  </tr>
                );
              },
              th({ children, ...props }) {
                return (
                  <th className="border border-base-300 px-2 py-1 text-left font-medium" {...props}>
                    {children}
                  </th>
                );
              },
              td({ children, ...props }) {
                return (
                  <td className="border border-base-300 px-2 py-1" {...props}>
                    {children}
                  </td>
                );
              },
            }}
          >
            {message.content || (isStreaming ? "" : "")}
          </ReactMarkdown>
        </div>

        {/* Action bar — hidden until hover, like VS Code */}
        {isAssistant && message.content && (
          <div className="flex items-center gap-0.5 opacity-0 group-hover:opacity-100 transition-opacity duration-150">
            <button
              type="button"
              onClick={copyMessage}
              className="btn btn-ghost btn-xs gap-1 text-base-content/40 hover:text-base-content"
              title="Copy response"
            >
              <Copy size={13} />
            </button>
            <button
              type="button"
              className="btn btn-ghost btn-xs btn-square text-base-content/40 hover:text-base-content"
              title="Helpful"
            >
              <ThumbsUp size={13} />
            </button>
            <button
              type="button"
              className="btn btn-ghost btn-xs btn-square text-base-content/40 hover:text-base-content"
              title="Not helpful"
            >
              <ThumbsDown size={13} />
            </button>
          </div>
        )}
      </div>
    </article>
  );
}
