import { useState, useEffect, useCallback } from "react";

interface WorkspaceGuideProps {
  step?: number;
  className?: string;
}

const workspaceIcons = ["🌐", "💼", "🎮", "📚"];
const workspaceNames = ["Default", "Work", "Games", "Research"];

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
      setAnimStep((prev) => (prev + 1) % 3);
    }, 3000);
    return () => clearInterval(timer);
  }, [autoPlay]);

  const handleStepClick = useCallback((s: number) => {
    setAnimStep(s);
    setAutoPlay(false);
  }, []);

  return (
    <div className={`flex flex-col gap-3 ${className}`}>
      {/* Mock browser window */}
      <div className="border border-base-300 rounded-xl overflow-hidden" style={{ aspectRatio: "16/10" }}>
        {/* Title bar */}
        <div className="bg-base-300/60 px-3 py-1.5 flex items-center gap-1.5">
          <div className="flex gap-1">
            <div className="w-2.5 h-2.5 rounded-full bg-error/60" />
            <div className="w-2.5 h-2.5 rounded-full bg-warning/60" />
            <div className="w-2.5 h-2.5 rounded-full bg-success/60" />
          </div>
        </div>

        {/* Main browser area */}
        <div className="flex" style={{ height: "calc(100% - 24px)" }}>
          {/* Sidebar with workspace icons */}
          <div className="w-10 bg-base-200 border-r border-base-300 flex flex-col items-center py-2 gap-1">
            {workspaceIcons.map((icon, i) => (
              <div
                key={i}
                className={`w-7 h-7 rounded-md flex items-center justify-center text-sm transition-all duration-500 ${
                  animStep === 0 && i === 0
                    ? "ring-2 ring-primary animate-pulse"
                    : animStep === 1 && i === 2
                      ? "ring-2 ring-primary animate-pulse"
                      : "opacity-50"
                }`}
              >
                {icon}
              </div>
            ))}
          </div>

          {/* Content */}
          <div className="flex-1 bg-base-100 relative">
            {/* Workspace popup (shown on step 1) */}
            {animStep === 1 && (
              <div className="absolute left-2 top-2 bg-base-200 border border-base-300 rounded-lg shadow-lg p-2 z-10 animate-fade-in">
                <div className="text-[10px] text-base-content/60 mb-1">Workspaces</div>
                {workspaceNames.map((name, i) => (
                  <div
                    key={i}
                    className={`flex items-center gap-1.5 px-2 py-1 rounded text-xs ${
                      i === 2 ? "bg-primary/20 text-primary" : "text-base-content/60"
                    }`}
                  >
                    <span>{workspaceIcons[i]}</span>
                    <span>{name}</span>
                  </div>
                ))}
              </div>
            )}

            {/* Tab content area */}
            <div className="p-3">
              {/* Tab bar */}
              <div className="flex items-center gap-1 mb-3">
                {animStep === 2
                  ? [2, 3].map((i) => (
                      <div
                        key={i}
                        className="px-2 py-0.5 bg-primary/10 rounded-t text-[10px] text-base-content/60"
                      >
                        {workspaceNames[i]} Tab
                      </div>
                    ))
                  : [0, 1].map((i) => (
                      <div
                        key={i}
                        className="px-2 py-0.5 bg-primary/10 rounded-t text-[10px] text-base-content/60"
                      >
                        {workspaceNames[i]} Tab
                      </div>
                    ))}
              </div>

              {/* Page content placeholder */}
              <div className="space-y-1.5">
                <div className="w-3/4 h-2 bg-base-content/8 rounded" />
                <div className="w-1/2 h-2 bg-base-content/8 rounded" />
                <div className="w-2/3 h-2 bg-base-content/8 rounded" />
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Step indicators */}
      <div className="flex items-center justify-center gap-2">
        {[0, 1, 2].map((s) => (
          <button
            key={s}
            type="button"
            onClick={() => handleStepClick(s)}
            className={`w-2 h-2 rounded-full transition-colors ${
              animStep === s ? "bg-primary" : "bg-base-300"
            }`}
          />
        ))}
      </div>
    </div>
  );
}
