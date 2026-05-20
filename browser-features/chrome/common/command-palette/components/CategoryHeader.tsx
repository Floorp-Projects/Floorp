// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";

interface CategoryHeaderProps {
  category: string;
}

export function CategoryHeader(props: CategoryHeaderProps) {
  const label = () =>
    i18next.t(`commandPalette.categories.${props.category}`, {
      defaultValue: props.category,
    });

  return (
    <div class="command-palette-category-header">
      {label()}
    </div>
  );
}
