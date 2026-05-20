import { useState, useEffect } from "react";

interface WorkspaceGuideProps {
  step?: number;
  className?: string;
}

const workspaceItems = [
  { icon: "🌐", name: "Default", tabs: ["GitHub", "Reddit"] },
  { icon: "💼", name: "Work", tabs: ["Slack", "Docs"] },
  { icon: "🎮", name: "Games", tabs: ["Steam", "YouTube"] },
  { icon: "📚", name: "Research", tabs: ["Wikipedia", "Arxiv"] },
];

const archivedItems = [
  { icon: "🗂️", name: "Old Project", tabs: 5, time: "2 days ago" },
  { icon: "🎓", name: "Study", tabs: 3, time: "1 week ago" },
];

const activeTabs = (step: number) =>
  step === 1 || step === 2 ? workspaceItems[2].tabs : workspaceItems[0].tabs;

export function WorkspaceGuide({ step: initialStep, className = "" }: WorkspaceGuideProps) {
  const [animStep, setAnimStep] = useState(initialStep ?? 0);
  const [autoPlay, setAutoPlay] = useState(initialStep === undefined);

  useEffect(() => {
    if (initialStep !== undefined) {
      setAnimStep(initialStep);
      setAutoPlay(false);
      return;
    }
    setAutoPlay(true);
  }, [initialStep]);

  useEffect(() => {
    if (!autoPlay) return;
    const timer = setInterval(() => {
      setAnimStep((prev) => (prev + 1) % 6);
    }, 3000);
    return () => clearInterval(timer);
  }, [autoPlay]);

  const isSidebarMode = animStep === 5;

  return (
    <div className={`flex flex-col gap-3 ${className}`}>
      {/* Mock browser window */}
      <div className="border border-base-300 rounded-xl overflow-hidden" style={{ aspectRatio: "16/6" }}>
        {/* Title bar */}
        <div className="bg-base-300/60 px-3 py-1.5 flex items-center gap-1.5">
          <div className="flex gap-1">
            <div className="w-2.5 h-2.5 rounded-full bg-error/60" />
            <div className="w-2.5 h-2.5 rounded-full bg-warning/60" />
            <div className="w-2.5 h-2.5 rounded-full bg-success/60" />
          </div>
        </div>

        {/* Tab strip with workspace button */}
        <div className="bg-base-200/60 px-2 py-1 flex items-center gap-1 border-b border-base-300">
          <div className={`px-1.5 py-0.5 rounded text-[10px] flex items-center gap-1 transition-all duration-500 ${
            isSidebarMode ? "opacity-30 line-through" : ""
          } ${animStep <= 1 ? "ring-1 ring-primary bg-primary/10" : "opacity-60"}`}>
            <span className="text-xs">🌐</span>
            <span className="text-[9px] text-base-content/60">Workspaces</span>
          </div>
          <div className="flex items-center gap-0.5 ml-2">
            {activeTabs(animStep).map((tab, i) => (
              <div
                key={`${animStep}-${i}`}
                className="px-2 py-0.5 bg-base-100 rounded-t text-[9px] text-base-content/50 transition-all duration-300"
              >
                {tab}
              </div>
            ))}
          </div>
        </div>

        {/* Main content area */}
        <div className="flex relative" style={{ height: "calc(100% - 36px)" }}>
          {/* Step 5: Sidebar mode — workspace icons in left rail */}
          {isSidebarMode && (
            <div className="w-8 bg-base-200 border-r border-base-300 flex flex-col items-center py-2 gap-1 animate-fade-in">
              {workspaceItems.map((ws, i) => (
                <div
                  key={i}
                  className={`w-6 h-6 rounded flex items-center justify-center text-[11px] transition-all duration-300 ${
                    i === 0 ? "ring-1 ring-primary bg-primary/10" : "opacity-50"
                  }`}
                >
                  {ws.icon}
                </div>
              ))}
              <div className="w-5 h-5 rounded border border-dashed border-base-content/30 flex items-center justify-center text-[9px] text-base-content/40">
                +
              </div>
            </div>
          )}

          {/* Step 0-1: Popup panel */}
          {(animStep === 0 || animStep === 1) && (
            <div className={`absolute left-2 top-2 bg-base-100 border border-base-300 rounded-lg shadow-xl z-10 animate-fade-in transition-all duration-300 ${animStep === 1 ? "w-[180px]" : "w-[160px]"}`}>
              <div className="p-1.5 space-y-0.5">
                {workspaceItems.map((ws, i) => (
                  <div
                    key={i}
                    className={`flex items-center gap-1.5 px-2 py-1 rounded text-[10px] transition-all duration-300 ${
                      (animStep === 0 && i === 0) || (animStep === 1 && i === 2)
                        ? "bg-primary/20 text-primary font-medium ring-1 ring-primary/30"
                        : "text-base-content/50"
                    }`}
                  >
                    <span>{ws.icon}</span>
                    <span>{ws.name}</span>
                  </div>
                ))}
              </div>
              <div className="border-t border-base-300 mx-1" />
              <div className="p-1.5 flex items-center gap-1">
                <div className="flex-1 flex items-center gap-1 px-1.5 py-0.5 rounded text-[9px] text-base-content/40">
                  <span className="text-[10px]">+</span>
                  <span>Create New</span>
                </div>
                <div className="w-5 h-5 rounded flex items-center justify-center text-[10px] text-base-content/40">📁</div>
                <div className="w-5 h-5 rounded flex items-center justify-center text-[10px] text-base-content/40">⚙️</div>
              </div>
            </div>
          )}

          {/* Step 2: Context menu */}
          {animStep === 2 && (
            <div className="absolute left-12 top-4 bg-base-100 border border-base-300 rounded-lg shadow-xl z-10 py-1 w-[120px] animate-fade-in">
              <div className="px-2 py-0.5 text-[9px] text-base-content/50">Move Up</div>
              <div className="px-2 py-0.5 text-[9px] text-base-content/50">Move Down</div>
              <div className="border-t border-base-300 mx-1 my-0.5" />
              <div className="px-2 py-0.5 text-[9px] text-error/70">Delete</div>
              <div className="px-2 py-0.5 text-[9px] text-base-content/70">Manage</div>
              <div className="border-t border-base-300 mx-1 my-0.5" />
              <div className="px-2 py-0.5 text-[9px] text-base-content/50">Archive</div>
            </div>
          )}

          {/* Step 3: Manage modal with container */}
          {animStep === 3 && (
            <div className="absolute inset-0 flex items-center justify-center bg-black/20 z-10 animate-fade-in">
              <div className="bg-base-100 border border-base-300 rounded-lg shadow-2xl w-[200px]">
                <div className="px-3 py-1.5 border-b border-base-300 text-[10px] font-semibold text-base-content/70">Edit Workspace</div>
                <div className="p-3 space-y-2">
                  <div>
                    <div className="text-[8px] text-base-content/50 mb-0.5">Name</div>
                    <div className="px-2 py-0.5 bg-base-200 rounded text-[9px]">Work</div>
                  </div>
                  <div>
                    <div className="text-[8px] text-base-content/50 mb-0.5">Container</div>
                    <div className="px-2 py-0.5 bg-base-200 rounded text-[9px] flex items-center gap-1">
                      <span className="w-2 h-2 rounded-full bg-orange-400" />
                      Work
                    </div>
                  </div>
                  <div>
                    <div className="text-[8px] text-base-content/50 mb-0.5">Icon</div>
                    <div className="px-2 py-0.5 bg-base-200 rounded text-[9px]">💼</div>
                  </div>
                </div>
                <div className="flex justify-end gap-1 px-3 py-1.5 border-t border-base-300">
                  <div className="px-2 py-0.5 text-[8px] text-base-content/50 rounded">Cancel</div>
                  <div className="px-2 py-0.5 text-[8px] bg-primary text-primary-content rounded">Save</div>
                </div>
              </div>
            </div>
          )}

          {/* Step 4: Restore mode popup */}
          {animStep === 4 && (
            <div className="absolute left-2 top-2 bg-base-100 border border-base-300 rounded-lg shadow-xl z-10 w-[200px] animate-fade-in">
              <div className="p-1.5 space-y-0.5">
                <div className="px-2 py-0.5 text-[9px] font-semibold text-base-content/60">Restored Workspaces</div>
                {archivedItems.map((item, i) => (
                  <div
                    key={i}
                    className="flex items-center gap-1.5 px-2 py-1 rounded text-[10px] text-base-content/60 hover:bg-base-200/50 transition-colors"
                  >
                    <span>{item.icon}</span>
                    <div className="flex-1 min-w-0">
                      <div className="truncate">{item.name}</div>
                      <div className="text-[8px] text-base-content/40">{item.tabs} tabs · {item.time}</div>
                    </div>
                  </div>
                ))}
              </div>
              <div className="border-t border-base-300 mx-1" />
              <div className="p-1.5 flex items-center gap-1">
                <div className="flex-1 flex items-center gap-1 px-1.5 py-0.5 rounded text-[9px] text-base-content/40">
                  <span className="text-[10px]">+</span>
                  <span>Create New</span>
                </div>
                <div className="w-5 h-5 rounded flex items-center justify-center text-[10px] text-primary/60 bg-primary/10 ring-1 ring-primary/30">📁</div>
                <div className="w-5 h-5 rounded flex items-center justify-center text-[10px] text-base-content/40">⚙️</div>
              </div>
            </div>
          )}

          {/* Page content placeholder */}
          <div className="flex-1 p-3">
            <div className="space-y-1.5">
              <div className="w-3/4 h-2 bg-base-content/8 rounded" />
              <div className="w-1/2 h-2 bg-base-content/8 rounded" />
              <div className="w-2/3 h-2 bg-base-content/8 rounded" />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
