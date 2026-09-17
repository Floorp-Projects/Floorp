import type { SectionProps } from "./types.ts";
import type { HTMLAttributes } from "react";
import styles from "./controls.module.css";

export function Card({ className = "", ...props }: SectionProps) {
  return <div className={`${styles.section} ${className}`} {...props} />;
}
export function CardHeader({ className = "", ...props }: SectionProps) {
  return <div className={`${styles.sectionHeader} ${className}`} {...props} />;
}
export function CardTitle(
  { className = "", ...props }: HTMLAttributes<HTMLHeadingElement>,
) {
  return <h2 className={`${styles.sectionTitle} ${className}`} {...props} />;
}
export function CardDescription(
  { className = "", ...props }: HTMLAttributes<HTMLParagraphElement>,
) {
  return (
    <p className={`${styles.sectionDescription} ${className}`} {...props} />
  );
}
export function CardContent({ className = "", ...props }: SectionProps) {
  return <div className={`${styles.sectionContent} ${className}`} {...props} />;
}
export function CardFooter({ className = "", ...props }: SectionProps) {
  return <div className={`${styles.sectionFooter} ${className}`} {...props} />;
}
