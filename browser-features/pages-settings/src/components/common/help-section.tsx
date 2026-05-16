import { useState } from "react";
import { ChevronDown, ChevronRight, HelpCircle } from "lucide-react";

interface HelpSectionProps {
  summary: string;
  children: React.ReactNode;
  defaultOpen?: boolean;
}

export function HelpSection({ summary, children, defaultOpen = false }: HelpSectionProps) {
  const [isOpen, setIsOpen] = useState(defaultOpen);

  return (
    <div className="border border-base-300 rounded-lg overflow-hidden">
      <button
        type="button"
        onClick={() => setIsOpen(!isOpen)}
        className="w-full flex items-center gap-2 px-3 py-2 text-sm text-base-content/80 hover:bg-base-200 transition-colors"
      >
        <HelpCircle size={16} className="text-info flex-shrink-0" />
        <span className="flex-1 text-left">{summary}</span>
        {isOpen ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
      </button>
      {isOpen && (
        <div className="px-3 py-2 text-sm text-base-content/70 border-t border-base-300 bg-base-200/50">
          {children}
        </div>
      )}
    </div>
  );
}
