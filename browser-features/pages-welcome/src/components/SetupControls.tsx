import { Alert, Field, NativeSelect } from "@chakra-ui/react";
import { Check, ChevronDown, Info } from "lucide-react";
import { useTranslation } from "react-i18next";
import type { PropsWithChildren } from "react";
import type { CurrentSettingProps, SetupSelectProps } from "./types.ts";
import styles from "../setup.module.css";

export function SetupSelect(
  { id, label, hint, icon, value, disabled, onChange, options }:
    SetupSelectProps,
) {
  return (
    <Field.Root unstyled disabled={disabled} className={styles.field}>
      <Field.Label htmlFor={id} className={styles.fieldLabel}>
        {label}
      </Field.Label>
      {hint && (
        <Field.HelperText className={styles.muted}>{hint}</Field.HelperText>
      )}
      <NativeSelect.Root
        unstyled
        disabled={disabled}
        className={styles.selectRoot}
        data-has-icon={icon ? "true" : undefined}
      >
        {icon && (
          <span className={styles.fieldIcon} aria-hidden="true">{icon}</span>
        )}
        <NativeSelect.Field
          id={id}
          value={value}
          onChange={(event) => onChange(event.target.value)}
          className={styles.select}
        >
          {options.map((item) => (
            <option key={item.value} value={item.value}>{item.label}</option>
          ))}
        </NativeSelect.Field>
        <NativeSelect.Indicator className={styles.selectIndicator}>
          <ChevronDown size={20} />
        </NativeSelect.Indicator>
      </NativeSelect.Root>
    </Field.Root>
  );
}

export function SelectionBadge() {
  const { t } = useTranslation();
  return (
    <span className={styles.selectionBadge}>
      <Check size={14} aria-hidden="true" />
      {t("setupRich.selected")}
    </span>
  );
}

export function CurrentSetting(
  { icon, label, value, detail }: CurrentSettingProps,
) {
  return (
    <div className={styles.currentSetting}>
      <span className={styles.currentAccent} aria-hidden="true" />
      <span className={styles.currentIcon} aria-hidden="true">{icon}</span>
      <div>
        <p className={styles.muted}>{label}</p>
        <strong>{value}</strong>
        {detail && <p className={styles.muted}>{detail}</p>}
      </div>
    </div>
  );
}

export function SetupInfo({ children }: PropsWithChildren) {
  return (
    <Alert.Root unstyled status="info" role="note" className={styles.info}>
      <Info size={22} aria-hidden="true" />
      <Alert.Content>{children}</Alert.Content>
    </Alert.Root>
  );
}
