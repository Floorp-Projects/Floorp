import { type ReactNode } from "react";

export function Grid({ children }: { children: ReactNode }) {
  return (
    <div className="container mx-auto max-w-6xl">
      <div className="flex flex-wrap gap-6">
        {children}
      </div>
    </div>
  );
}
