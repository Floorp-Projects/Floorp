import { Clock } from "./Clock/index.tsx";
import { TopSites } from "./TopSites/index.tsx";
import { SearchBar } from "./SearchBar/index.tsx";
import { useComponents } from "@/contexts/ComponentsContext.tsx";

export function DefaultLayout() {
  const { components } = useComponents();

  return (
    <>
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 p-4 w-full">
        <div className="lg:col-span-2 mb-10">
          {components.topSites && <TopSites />}
        </div>
        <div className="flex justify-center lg:justify-end h-fit">
          {components.clock && <Clock />}
        </div>
      </div>
      <div className="w-full flex justify-center px-4">
        <div className="w-full max-w-full sm:max-w-lg md:max-w-xl lg:max-w-2xl mt-8 md:mt-16">
          {components.searchBar && <SearchBar />}
        </div>
      </div>
    </>
  );
}
