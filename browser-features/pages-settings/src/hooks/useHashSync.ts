import { useEffect } from "react";

export default function useHashSync(pathname: string) {
  useEffect(() => {
    if (pathname) {
      const currentHash = globalThis.location.hash.slice(1);
      if (currentHash !== pathname) {
        globalThis.location.hash = pathname;
        document.documentElement.dataset.isRouteChanged = "true";
      }
    }
  }, [pathname]);
}
