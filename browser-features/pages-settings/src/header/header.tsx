import { SidebarTrigger } from "@/components/common/sidebar.tsx";
import { SettingsSearchInput } from "./SearchInput.tsx";
export function Header() {
  return (
    <header className="floorp-settings-header">
      <SidebarTrigger />
      <div>
        <SettingsSearchInput />
      </div>
    </header>
  );
}
