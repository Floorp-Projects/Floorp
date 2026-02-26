import { useEffect, useRef, useState } from "react";
import {
  AlertCircle,
  ChevronDown,
  MessageSquarePlus,
  PanelLeftClose,
  PanelLeftOpen,
  Paperclip,
  Send,
  Settings,
  Sparkles,
  Trash2,
  Wrench,
  X,
} from "lucide-react";
import { type Message, type ToolRun } from "../types.ts";
import { ChatMessage } from "./ChatMessage.tsx";
import {
  getAllEnabledProvidersAsync,
  type LLMProviderSettings,
  runAgenticLoopWithStream,
} from "../lib/llm.ts";

// Tool call display type
interface ToolCallInfo {
  id: string;
  name: string;
  args: unknown;
  status: "pending" | "running" | "completed";
}

// Chat session type
interface ChatSession {
  id: string;
  title: string;
  messages: Message[];
  createdAt: number;
  updatedAt: number;
}

const CHAT_SESSIONS_STORAGE_KEY = "floorp-chat-sessions";

// Safe localStorage with fallback for private browsing mode
function safeGetItem(key: string): string | null {
  try {
    return localStorage.getItem(key);
  } catch (e) {
    console.warn("[Chat] localStorage unavailable, using memory:", e);
    return null;
  }
}

function safeSetItem(key: string, value: string): boolean {
  try {
    localStorage.setItem(key, value);
    return true;
  } catch (e) {
    console.warn("[Chat] localStorage write failed:", e);
    return false;
  }
}

// Reserved for future use
function _safeRemoveItem(key: string): boolean {
  try {
    localStorage.removeItem(key);
    return true;
  } catch (e) {
    console.warn("[Chat] localStorage remove failed:", e);
    return false;
  }
}

function persistChatSessions(sessions: ChatSession[]): void {
  safeSetItem(CHAT_SESSIONS_STORAGE_KEY, JSON.stringify(sessions));
}

function truncateToolResult(result: string, max = 240): string {
  const compact = result.replace(/\s+/g, " ").trim();
  return compact.length > max ? `${compact.slice(0, max - 1)}...` : compact;
}

function upsertToolRun(
  runs: ToolRun[] | undefined,
  run: ToolRun,
): ToolRun[] {
  const current = runs ?? [];
  const existingIndex = current.findIndex((item) => item.id === run.id);
  if (existingIndex === -1) {
    return [...current, run];
  }

  const next = [...current];
  next[existingIndex] = { ...next[existingIndex], ...run };
  return next;
}

export function Chat() {
  const [messages, setMessages] = useState<Message[]>([]);
  const [input, setInput] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const [isGenerating, setIsGenerating] = useState(false); // LLM is generating text
  const [isRunningTool, setIsRunningTool] = useState(false); // Tool is executing
  const [error, setError] = useState<string | null>(null);
  const [currentToolCall, setCurrentToolCall] = useState<ToolCallInfo | null>(
    null,
  );
  const [providers, setProviders] = useState<LLMProviderSettings[]>([]);
  const [selectedProvider, setSelectedProvider] = useState<
    LLMProviderSettings | null
  >(null);
  const [showProviderDropdown, setShowProviderDropdown] = useState(false);
  const [showHistory, setShowHistory] = useState(false);
  const [chatSessions, setChatSessions] = useState<ChatSession[]>([]);
  const [currentSessionId, setCurrentSessionId] = useState<string | null>(null);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);
  const toolCallStatusTimerRef = useRef<number | null>(null);
  const toolCallStatusTokenRef = useRef(0);
  const inMemorySessions = useRef<Map<string, ChatSession>>(new Map());

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: "smooth" });
  };

  // Load providers on mount
  useEffect(() => {
    const loadProviders = async () => {
      try {
        const enabledProviders = await getAllEnabledProvidersAsync();
        setProviders(enabledProviders);
        setSelectedProvider((prev) => prev ?? enabledProviders[0] ?? null);
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        setError(message);
      }
    };
    loadProviders();
  }, []);

  // Listen for provider changes from settings
  useEffect(() => {
    const handleStorageChange = (e: StorageEvent) => {
      if (e.key === "floorp.llm.providers") {
        // Reload providers when settings change
        getAllEnabledProvidersAsync().then((enabledProviders) => {
          setProviders(enabledProviders);
          if (selectedProvider) {
            const updated = enabledProviders.find(
              (p) => p.type === selectedProvider.type,
            );
            if (updated) setSelectedProvider(updated);
          }
        }).catch((err) => {
          console.error("[Chat] Failed to reload providers:", err);
        });
      }
    };

    window.addEventListener("storage", handleStorageChange);
    return () => window.removeEventListener("storage", handleStorageChange);
  }, [selectedProvider]);

  // Load chat sessions from localStorage (safe version)
  useEffect(() => {
    const saved = safeGetItem(CHAT_SESSIONS_STORAGE_KEY);
    if (saved) {
      try {
        const sessions = JSON.parse(saved) as ChatSession[];
        setChatSessions(sessions);
        // Load most recent session
        if (sessions.length > 0) {
          const latest = [...sessions].sort((a, b) =>
            b.updatedAt - a.updatedAt
          )[0];
          setCurrentSessionId(latest.id);
          setMessages(latest.messages);
        }
      } catch {
        console.error("Failed to load chat sessions");
      }
    } else {
      // Load from in-memory storage as fallback
      const memorySessions = Array.from(inMemorySessions.current.values());
      if (memorySessions.length > 0) {
        setChatSessions(memorySessions);
        const latest = memorySessions.sort((a, b) =>
          b.updatedAt - a.updatedAt
        )[0];
        setCurrentSessionId(latest.id);
        setMessages(latest.messages);
      }
    }
  }, []);

  // Save current session when messages change
  useEffect(() => {
    if (messages.length > 0 && currentSessionId) {
      setChatSessions((prev) => {
        const updated = prev.map((s) =>
          s.id === currentSessionId
            ? {
              ...s,
              messages,
              updatedAt: Date.now(),
              title: messages[0]?.content?.slice(0, 30) || "New Chat",
            }
            : s
        );
        persistChatSessions(updated);
        return updated;
      });
    }
  }, [messages, currentSessionId]);

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  useEffect(() => {
    const textarea = inputRef.current;
    if (!textarea) return;

    textarea.style.height = "0px";
    const nextHeight = Math.min(textarea.scrollHeight, 280);
    textarea.style.height = `${Math.max(120, nextHeight)}px`;
  }, [input]);

  useEffect(() => {
    if (!showProviderDropdown) return;

    const onPointerDown = (event: MouseEvent) => {
      const target = event.target as Node | null;
      if (!target) return;
      const dropdown = document.getElementById("llm-provider-dropdown");
      const trigger = document.getElementById("llm-provider-trigger");
      if (dropdown?.contains(target) || trigger?.contains(target)) {
        return;
      }
      setShowProviderDropdown(false);
    };

    document.addEventListener("mousedown", onPointerDown);
    return () => document.removeEventListener("mousedown", onPointerDown);
  }, [showProviderDropdown]);

  useEffect(() => {
    return () => {
      if (toolCallStatusTimerRef.current !== null) {
        globalThis.clearTimeout(toolCallStatusTimerRef.current);
      }
    };
  }, []);

  // Create new chat session
  const createNewChat = () => {
    const newSession: ChatSession = {
      id: crypto.randomUUID(),
      title: "New Chat",
      messages: [],
      createdAt: Date.now(),
      updatedAt: Date.now(),
    };
    setChatSessions((prev) => {
      const updated = [newSession, ...prev];
      persistChatSessions(updated);
      return updated;
    });
    setCurrentSessionId(newSession.id);
    setMessages([]);
    setError(null);
  };

  // Load a chat session
  const loadSession = (session: ChatSession) => {
    setCurrentSessionId(session.id);
    setMessages(session.messages);
    setError(null);
    setShowHistory(false);
  };

  // Delete a chat session
  const deleteSession = (sessionId: string) => {
    setChatSessions((prev) => {
      const updated = prev.filter((s) => s.id !== sessionId);
      persistChatSessions(updated);
      return updated;
    });
    if (currentSessionId === sessionId) {
      setCurrentSessionId(null);
      setMessages([]);
    }
  };

  // Clear current chat
  const clearCurrentChat = () => {
    setMessages([]);
    setError(null);
    if (currentSessionId) {
      setChatSessions((prev) => {
        const updated = prev.map((s) =>
          s.id === currentSessionId
            ? { ...s, messages: [], title: "New Chat" }
            : s
        );
        persistChatSessions(updated);
        return updated;
      });
    }
  };

  const handleSubmit = async (e?: React.FormEvent) => {
    e?.preventDefault();
    const trimmedInput = input.trim();
    if (!trimmedInput || isLoading) return;

    const provider = selectedProvider;
    if (!provider) {
      setError(
        "No LLM provider configured. Please set up a provider in settings.",
      );
      return;
    }

    // Create new session if none exists
    if (!currentSessionId) {
      const newSession: ChatSession = {
        id: crypto.randomUUID(),
        title: trimmedInput.slice(0, 30),
        messages: [],
        createdAt: Date.now(),
        updatedAt: Date.now(),
      };
      setChatSessions((prev) => {
        const updated = [newSession, ...prev];
        persistChatSessions(updated);
        return updated;
      });
      setCurrentSessionId(newSession.id);
    }

    const userMessage: Message = {
      id: crypto.randomUUID(),
      role: "user",
      content: trimmedInput,
      timestamp: Date.now(),
    };

    const assistantMessage: Message = {
      id: crypto.randomUUID(),
      role: "assistant",
      content: "",
      timestamp: Date.now(),
      provider: provider.type,
      model: provider.defaultModel,
      toolRuns: [],
    };

    setMessages((prev) => [...prev, userMessage, assistantMessage]);
    setInput("");
    setError(null);
    setIsLoading(true);
    setIsGenerating(true);
    setIsRunningTool(false);
    setCurrentToolCall(null);

    try {
      const allMessages = [...messages, userMessage];

      // Run agentic loop with streaming support
      await runAgenticLoopWithStream(
        provider,
        allMessages,
        {
          onStreamStart: () => {
            setIsGenerating(true);
            setIsRunningTool(false);
          },
          onContent: (content) => {
            setMessages((prev) =>
              prev.map((m) =>
                m.id === assistantMessage.id ? { ...m, content } : m
              )
            );
          },
          onStreamEnd: () => {
            setIsGenerating(false);
          },
          onToolCallStart: ({ id, name, args }) => {
            setIsGenerating(false);
            setIsRunningTool(true);
            toolCallStatusTokenRef.current += 1;
            if (toolCallStatusTimerRef.current !== null) {
              globalThis.clearTimeout(toolCallStatusTimerRef.current);
              toolCallStatusTimerRef.current = null;
            }

            setMessages((prev) =>
              prev.map((m) =>
                m.id === assistantMessage.id
                  ? {
                    ...m,
                    toolRuns: upsertToolRun(m.toolRuns, {
                      id,
                      name,
                      args,
                      status: "running",
                      startedAt: Date.now(),
                    }),
                  }
                  : m
              )
            );

            setCurrentToolCall({
              id,
              name,
              args,
              status: "running",
            });
          },
          onToolCallEnd: ({ id, name, args, result, isError }) => {
            const token = toolCallStatusTokenRef.current;
            const finishedAt = Date.now();
            const nextToolRunStatus: ToolRun["status"] = isError
              ? "error"
              : "completed";
            setCurrentToolCall((prev) =>
              prev?.id === id
                ? {
                  ...prev,
                  name,
                  args,
                  status: "completed",
                }
                : prev
            );
            if (toolCallStatusTimerRef.current !== null) {
              globalThis.clearTimeout(toolCallStatusTimerRef.current);
            }
            toolCallStatusTimerRef.current = globalThis.setTimeout(() => {
              setCurrentToolCall((prev) =>
                toolCallStatusTokenRef.current === token && prev?.id === id
                  ? null
                  : prev
              );
              toolCallStatusTimerRef.current = null;
            }, 900);
            setMessages((prev) =>
              prev.map((m) => {
                if (m.id !== assistantMessage.id) return m;
                const toolRuns: ToolRun[] = (m.toolRuns ?? []).map((
                  run,
                ): ToolRun =>
                  run.id === id
                    ? {
                      ...run,
                      name,
                      args,
                      status: nextToolRunStatus,
                      finishedAt,
                      resultPreview: truncateToolResult(result),
                    }
                    : run
                );
                return { ...m, toolRuns };
              })
            );
          },
          onError: (error) => {
            setError(error.message);
          },
        },
        5, // max iterations
      );
    } catch (err) {
      setError(err instanceof Error ? err.message : "An error occurred");
      // Remove the placeholder assistant message only if it stayed empty.
      setMessages((prev) =>
        prev.filter((m) => !(m.id === assistantMessage.id && !m.content))
      );
    } finally {
      toolCallStatusTokenRef.current += 1;
      if (toolCallStatusTimerRef.current !== null) {
        globalThis.clearTimeout(toolCallStatusTimerRef.current);
        toolCallStatusTimerRef.current = null;
      }
      setIsLoading(false);
      setIsGenerating(false);
      setIsRunningTool(false);
      setCurrentToolCall(null);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      handleSubmit();
    }
  };

  const openSettings = () => {
    // Open Floorp settings page for LLM providers
    globalThis.open("floorp://settings/llm-providers", "_blank");
  };

  const latestMessageId = messages[messages.length - 1]?.id;

  return (
    <div className="flex h-screen bg-base-100 text-base-content">
      {/* ── History sidebar ──────────────────────────────── */}
      {showHistory && (
        <aside className="w-64 flex flex-col border-r border-base-300 bg-base-200/60">
          <div className="flex items-center justify-between px-3 py-2 border-b border-base-300">
            <span className="text-xs font-semibold text-base-content/60 uppercase tracking-wider">
              History
            </span>
            <button
              type="button"
              onClick={() => setShowHistory(false)}
              className="btn btn-ghost btn-xs btn-circle text-base-content/50"
              title="Close history"
            >
              <X size={14} />
            </button>
          </div>
          <div className="flex-1 overflow-y-auto p-1.5 space-y-0.5">
            {chatSessions.length === 0
              ? (
                <p className="py-8 text-center text-xs text-base-content/40">
                  No conversations yet
                </p>
              )
              : (
                chatSessions.map((session) => (
                  <div
                    key={session.id}
                    role="button"
                    tabIndex={0}
                    onClick={() => loadSession(session)}
                    onKeyDown={(e) => {
                      if (e.key === "Enter" || e.key === " ") {
                        e.preventDefault();
                        loadSession(session);
                      }
                    }}
                    className={`group flex w-full items-center gap-2 rounded-lg px-2.5 py-2 text-left transition-colors ${
                      currentSessionId === session.id
                        ? "bg-primary/10 text-primary"
                        : "text-base-content/70 hover:bg-base-300/60"
                    }`}
                  >
                    <MessageSquarePlus
                      size={13}
                      className="shrink-0 opacity-60"
                    />
                    <span className="flex-1 truncate text-[13px]">
                      {session.title}
                    </span>
                    <button
                      type="button"
                      onClick={(e) => {
                        e.stopPropagation();
                        deleteSession(session.id);
                      }}
                      onKeyDown={(e) => {
                        if (e.key === "Enter" || e.key === " ") {
                          e.preventDefault();
                          e.stopPropagation();
                          deleteSession(session.id);
                        }
                      }}
                      className="opacity-0 group-hover:opacity-100 btn btn-ghost btn-xs btn-circle text-error/70 hover:text-error"
                      title="Delete session"
                    >
                      <Trash2 size={11} />
                    </button>
                  </div>
                ))
              )}
          </div>
        </aside>
      )}

      <div className="flex min-w-0 flex-1 flex-col">
        {/* ── Header ───────────────────────────────────────── */}
        <header className="flex items-center gap-2 border-b border-base-300 bg-base-100 px-3 py-1.5">
          <div className="flex items-center gap-0.5">
            <button
              type="button"
              onClick={() => setShowHistory(!showHistory)}
              className={`btn btn-ghost btn-sm btn-square ${
                showHistory ? "text-primary" : "text-base-content/50"
              }`}
              title="Toggle history"
            >
              {showHistory
                ? <PanelLeftClose size={16} />
                : <PanelLeftOpen size={16} />}
            </button>
            <button
              type="button"
              onClick={createNewChat}
              className="btn btn-ghost btn-sm btn-square text-base-content/50"
              title="New Chat"
            >
              <MessageSquarePlus size={16} />
            </button>
          </div>

          <div className="flex-1 min-w-0 flex items-center gap-2">
            <Sparkles size={14} className="shrink-0 text-primary" />
            <span className="truncate text-sm font-semibold">Floorp Agent</span>
          </div>

          <div className="flex items-center gap-0.5">
            {messages.length > 0 && (
              <button
                type="button"
                onClick={clearCurrentChat}
                className="btn btn-ghost btn-sm btn-square text-base-content/50"
                title="Clear Chat"
              >
                <Trash2 size={15} />
              </button>
            )}
            <button
              type="button"
              onClick={openSettings}
              className="btn btn-ghost btn-sm btn-square text-base-content/50"
              title="Settings"
            >
              <Settings size={15} />
            </button>
          </div>
        </header>

        {/* ── Messages ─────────────────────────────────────── */}
        <div className="flex-1 overflow-y-auto">
          <div className="flex w-full flex-col px-5 py-4">
            {/* Empty state */}
            {messages.length === 0 && (
              <div className="flex flex-1 flex-col items-center justify-center py-20 text-center">
                <div className="mb-3 flex h-10 w-10 items-center justify-center rounded-lg bg-primary/10">
                  <Sparkles size={20} className="text-primary" />
                </div>
                <h2 className="text-base font-semibold text-base-content mb-1">
                  Floorp Agent
                </h2>
                <p className="text-[13px] text-base-content/50 max-w-md mb-5">
                  Ask anything. The agent can search, navigate, and inspect
                  pages with tools.
                </p>
                <div className="flex flex-wrap justify-center gap-1.5">
                  {[
                    "Open example.com and summarize",
                    "Search for Floorp release notes",
                    "Extract all headings from a page",
                  ].map((suggestion) => (
                    <button
                      key={suggestion}
                      type="button"
                      onClick={() => setInput(suggestion)}
                      className="btn btn-sm btn-ghost border border-base-300 text-[12px] text-base-content/60 font-normal"
                    >
                      {suggestion}
                    </button>
                  ))}
                </div>
              </div>
            )}

            {messages.map((message) => (
              <ChatMessage
                key={message.id}
                message={message}
                isStreaming={isLoading &&
                  message.role === "assistant" &&
                  message.id === latestMessageId}
              />
            ))}

            {/* Thinking indicator — minimal, inline */}
            {isLoading && !isGenerating && !isRunningTool &&
              messages[messages.length - 1]?.content === "" &&
              !currentToolCall && (
              <div className="flex items-center gap-2 py-3 pl-8 text-[13px] text-base-content/50">
                <span className="loading loading-dots loading-xs" />
                <span>Thinking…</span>
              </div>
            )}

            {/* Generating indicator — distinct from tool running */}
            {isGenerating &&
              messages[messages.length - 1]?.id === latestMessageId && (
              <div className="flex items-center gap-2 py-3 pl-8 text-[13px] text-base-content/50">
                <span className="loading loading-dots loading-xs" />
                <span>Generating…</span>
              </div>
            )}

            {/* Tool running indicator with details */}
            {isRunningTool && currentToolCall && (
              <div className="flex flex-col gap-1 py-2 pl-8 text-[12px] text-base-content/50">
                <div className="flex items-center gap-2">
                  <Wrench
                    size={13}
                    className={currentToolCall.status === "running"
                      ? "animate-spin text-primary"
                      : "text-success"}
                  />
                  <span className="truncate">
                    {currentToolCall.status === "running"
                      ? `Running ${currentToolCall.name}…`
                      : `Completed ${currentToolCall.name}`}
                  </span>
                </div>
                {currentToolCall.args && (
                  <div className="ml-5 text-[11px] text-base-content/40 truncate max-w-md">
                    {JSON.stringify(currentToolCall.args)}
                  </div>
                )}
              </div>
            )}

            <div ref={messagesEndRef} />
          </div>
        </div>

        {/* ── Error ──────────────────────────────────────── */}
        {error && (
          <div className="px-4 pb-2">
            <div
              role="alert"
              className="alert alert-error alert-soft text-sm py-2"
            >
              <AlertCircle size={14} />
              <span>{error}</span>
            </div>
          </div>
        )}

        {/* ── Input ──────────────────────────────────────── */}
        <form
          onSubmit={handleSubmit}
          className="border-t border-base-300 bg-base-100 px-5 py-3"
        >
          <div className="relative">
            <textarea
              ref={inputRef}
              value={input}
              onChange={(e) => setInput(e.target.value)}
              onKeyDown={handleKeyDown}
              placeholder="Ask anything…"
              rows={3}
              className="textarea textarea-bordered w-full resize-none text-[13px] leading-relaxed pr-20"
              style={{ minHeight: "120px", maxHeight: "280px" }}
            />
            <div className="absolute right-2 bottom-2 flex items-center gap-1">
              <button
                type="button"
                className="btn btn-ghost btn-xs btn-square text-base-content/25 hover:text-base-content/60"
                title="Attachments (planned)"
              >
                <Paperclip size={14} />
              </button>
              <button
                type="submit"
                disabled={!input.trim() || isLoading}
                className="btn btn-primary btn-sm h-8 w-8 p-0"
                title="Send"
              >
                {isLoading
                  ? <span className="loading loading-spinner loading-xs" />
                  : <Send size={14} />}
              </button>
            </div>
          </div>

          {/* Provider selector & info — below input */}
          <div className="mt-2 flex items-center gap-2 text-[11px]">
            {providers.length > 0 && (
              <div className="relative">
                <button
                  id="llm-provider-trigger"
                  type="button"
                  onClick={() => setShowProviderDropdown(!showProviderDropdown)}
                  className="inline-flex items-center gap-1 rounded-md border border-base-300 bg-base-200/60 px-2 py-1 text-[11px] text-base-content/60 hover:bg-base-300/60 transition-colors"
                >
                  <span className="font-mono">
                    {selectedProvider?.type || "Provider"}
                    {selectedProvider?.defaultModel
                      ? ` · ${selectedProvider.defaultModel}`
                      : ""}
                  </span>
                  <ChevronDown size={11} />
                </button>
                {showProviderDropdown && (
                  <ul
                    id="llm-provider-dropdown"
                    className="absolute bottom-full left-0 z-50 mb-1 menu p-1.5 shadow-lg bg-base-100 rounded-lg w-56 border border-base-300"
                  >
                    {providers.map((provider) => (
                      <li key={provider.type}>
                        <button
                          type="button"
                          onClick={() => {
                            setSelectedProvider(provider);
                            setShowProviderDropdown(false);
                          }}
                          className={`text-sm ${
                            selectedProvider?.type === provider.type
                              ? "active"
                              : ""
                          }`}
                        >
                          <div>
                            <div className="font-medium">{provider.type}</div>
                            {provider.defaultModel && (
                              <div className="text-[11px] text-base-content/50">
                                {provider.defaultModel}
                              </div>
                            )}
                          </div>
                        </button>
                      </li>
                    ))}
                  </ul>
                )}
              </div>
            )}
            {providers.length === 0 && (
              <span className="font-mono text-base-content/30">
                no provider
              </span>
            )}
            <span className="ml-auto text-[10px] text-base-content/30 hidden sm:inline">
              Enter ↵ send · Shift+Enter newline
            </span>
          </div>
        </form>
      </div>
    </div>
  );
}
