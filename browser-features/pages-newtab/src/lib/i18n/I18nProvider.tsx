import { PropsWithChildren, useEffect, useState } from "react";
import { initializeI18n } from "@/lib/i18n/init.ts";

export function I18nProvider({ children }: PropsWithChildren) {
  const [ready, setReady] = useState(false);

  useEffect(() => {
    let mounted = true;
    (async () => {
      try {
        await initializeI18n();
      } catch (e) {
        console.error("[I18nProvider] initialization failed:", e);
      }
      if (mounted) setReady(true);
    })();
    return () => {
      mounted = false;
    };
  }, []);

  if (!ready) {
    return <div />;
  }

  return <>{children}</>;
}
