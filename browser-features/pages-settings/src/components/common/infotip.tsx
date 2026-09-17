import { Info } from "lucide-react";
import { Portal, Tooltip } from "@chakra-ui/react";
import { Button } from "./button.tsx";

export function InfoTip({ description }: { description: string }) {
  return (
    <Tooltip.Root>
      <Tooltip.Trigger asChild>
        <Button variant="ghost" aria-label={description}>
          <Info size={20} aria-hidden="true" />
        </Button>
      </Tooltip.Trigger>
      <Portal>
        <Tooltip.Positioner>
          <Tooltip.Content className="floorp-tooltip">
            {description}
          </Tooltip.Content>
        </Tooltip.Positioner>
      </Portal>
    </Tooltip.Root>
  );
}
