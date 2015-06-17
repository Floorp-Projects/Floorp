<transform xmlns="http://www.w3.org/1999/XSL/Transform" version="1.0">
	<template match="node()|@*">
		<copy>
			<apply-templates select="@*"/>
			<apply-templates/>
		</copy>
	</template>
	<template match="@fill">
		<attribute name="fill">lime</attribute>
	</template>
</transform>
