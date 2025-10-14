import { Clock } from "./Clock/index.tsx";
import { TopSites } from "./TopSites/index.tsx";
import { SearchBar } from "./SearchBar/index.tsx";
import { FirefoxLayout } from "./FirefoxLayout/index.tsx";
import { useComponents } from "@/contexts/ComponentsContext.tsx";

export function FirefoxNewTabLayout() {
  const { components } = useComponents();

  return (
    <div className="w-full min-h-screen flex flex-col justify-center items-center p-4">
      <div className="absolute top-4 right-4">
        {components.clock && <Clock />}
      </div>
      <div className="flex flex-col items-center w-full">
        {components.searchBar && (
          <div className="w-full max-w-full sm:max-w-lg md:max-w-xl lg:max-w-2xl mb-12 sm:mb-16 md:mb-20 lg:mb-24 ">
            <FirefoxLayout />
            <SearchBar />
          </div>
        )}
        <div className="max-w-md sm:max-w-lg md:max-w-xl lg:max-w-5xl">
          {components.topSites && (
            <TopSites isFirefoxMode={components.firefoxLayout} />
          )}
        </div>
      </div>
    </div>
  );
}
