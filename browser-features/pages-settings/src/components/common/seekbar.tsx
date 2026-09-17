import { Flex, Slider, Text } from "@chakra-ui/react";
import { useId } from "react";
import type { SeekbarProps } from "./types.ts";

export function Seekbar({
  label,
  description,
  className,
  showValue = true,
  showMinMax = true,
  minLabel,
  maxLabel,
  size = "md",
  valuePrefix = "",
  valueSuffix = "",
  min = 0,
  max = 100,
  step = 1,
  value = 0,
  disabled = false,
  onValueChange,
  onMouseEnter,
  onMouseLeave,
}: SeekbarProps) {
  const descriptionId = useId();
  return (
    <Slider.Root
      className={className}
      colorPalette="purple"
      size={size}
      value={[value]}
      min={min}
      max={max}
      step={step}
      disabled={disabled}
      onValueChange={({ value }) => onValueChange?.(value[0])}
      onMouseEnter={onMouseEnter}
      onMouseLeave={onMouseLeave}
    >
      <Flex justify="space-between" gap="4">
        <Slider.Label>{label}</Slider.Label>
        {showValue && (
          <Text textStyle="sm">{valuePrefix}{value}{valueSuffix}</Text>
        )}
      </Flex>
      {description && (
        <Text id={descriptionId} color="fg.muted" textStyle="sm">
          {description}
        </Text>
      )}
      <Slider.Control>
        <Slider.Track>
          <Slider.Range />
        </Slider.Track>
        <Slider.Thumb
          index={0}
          aria-describedby={description ? descriptionId : undefined}
          aria-valuetext={`${valuePrefix}${value}${valueSuffix}`}
        >
          <Slider.HiddenInput />
        </Slider.Thumb>
      </Slider.Control>
      {showMinMax && (
        <Flex
          justify="space-between"
          color="fg.muted"
          textStyle="sm"
          aria-hidden="true"
        >
          <span>{minLabel ?? min}</span>
          <span>{maxLabel ?? max}</span>
        </Flex>
      )}
    </Slider.Root>
  );
}
